#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>

struct FlowNode
{
    juce::Point<float> position;
    float value    = 0.0f;
    float energy   = 0.0f;
    float activity = 0.0f;

    std::atomic<float>* param = nullptr;
    juce::String paramID;
    int slotIndex = -1; // Cached Slot Index for faster lookup

    bool isDragging = false;
};

struct FlowParticle
{
    float t     = 0.0f;   // Position along the line (0 -> 1)
    float speed = 0.2f;   // Base movement speed
    std::vector<float> history; // Trails
};

struct NodeConnection
{
    int   source = -1;
    int   target = -1;
    int   targetSlot = -1; // Cached Parameter Slot Index
    float amount = 0.5f;   // User-defined base amount
    float health = 1.0f;   // Link integrity (0.0 -> 1.0)
    bool  isPersistent = false; // If true, won't decay (future use)

    std::vector<FlowParticle> particles;
};

struct NodeSystemSnapshot
{
    std::vector<FlowNode> nodes;
    std::vector<NodeConnection> connections;
};

class NodeSystem
{
public:
    static juce::Point<float> getDefaultNodePosition(int index)
    {
        static const std::array<juce::Point<float>, 16> layout {{
            { 0.26f, 0.52f }, { 0.40f, 0.20f }, { 0.63f, 0.22f }, { 0.78f, 0.46f },
            { 0.66f, 0.70f }, { 0.44f, 0.80f }, { 0.23f, 0.66f }, { 0.51f, 0.43f },
            { 0.41f, 0.57f }, { 0.58f, 0.60f }, { 0.70f, 0.82f }, { 0.84f, 0.56f },
            { 0.31f, 0.36f }, { 0.51f, 0.87f }, { 0.59f, 0.34f }, { 0.36f, 0.72f }
        }};

        return layout[(size_t) juce::jlimit(0, (int) layout.size() - 1, index)];
    }

    void initialise(juce::AudioProcessorValueTreeState& tree)
    {
        const juce::ScopedLock sl (lock);
        nodes.clear();
        connections.clear();

        static constexpr std::array<const char*, 16> params {{
            "sourceDepth", "sourceCore", "sourceAnchor",
            "flowRate", "flowEnergy", "flowTexture",
            "pulseFrequency", "pulseWidth", "pulseEnergy",
            "fieldComplexity", "fieldDepth", "instability", "systemMatrix",
            "radiance", "charge", "discharge"
        }};
        static constexpr std::array<int, 16> modulationSlotIndices {{
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
            9, 10, 11, 15,
            12, 13, 14
        }};

        for (size_t i = 0; i < params.size(); ++i)
        {
            FlowNode n;
            n.paramID = params[i];
            n.param   = tree.getRawParameterValue(params[i]);
            // The visual node order keeps the legacy season/system node ahead of the FX trio,
            // while slot indices still target the processor's modulation array order.
            n.slotIndex = modulationSlotIndices[i];
            if (n.param) n.value = n.param->load();
            n.position  = getDefaultNodePosition((int) i);

            nodes.push_back(n);
        }
    }

    void update(float rms, float time, float beatPhase)
    {
        const juce::ScopedLock sl (lock);
        for (int i = 0; i < (int)nodes.size(); ++i)
        {
            auto& n = nodes[i];
            if (n.param)
                n.value = n.param->load();

            // Pulse based on beat
            float bPulse = std::pow(std::sin(beatPhase * juce::MathConstants<float>::pi) * 0.5f + 0.5f, 2.0f);
            n.energy   = 0.85f * n.energy + 0.15f * (rms * 1.5f + n.value * 0.4f + bPulse * 0.3f);
            
            // cybernetic metabolic pulse
            float speed = 0.4f + n.value * 1.2f; 
            n.activity  = 0.5f + 0.5f * std::sin(time * speed + float(i));
            
            // Cybernetic Drift & Parameter Sync
            if (!n.isDragging)
            {
                // 1. Sync Y-position to the parameter value (1.0 at top, 0.0 at bottom)
                // We use a slightly more organic mapping that preserves some vertical layout bias
                float targetY = 1.0f - n.value;
                
                // Springy interpolation to make the movement feel 'solid' and 'plastisch'
                n.position.y += (targetY - n.position.y) * 0.08f;

                // 2. Slow cybernetic drift for X
                n.position.x = juce::jlimit(0.05f, 0.95f,
                    n.position.x + std::sin(time * 0.22f + (float)i * 1.3f) * 0.0003f);
                
                // 3. Add 'Energy' jitter to make them look active when processed
                n.position.x += std::cos(time * 1.5f + (float)i) * n.energy * 0.0008f;
                n.position.y += std::sin(time * 1.8f + (float)i * 0.5f) * n.energy * 0.0005f;
            }
        }

        // Decay and Growth for connections (Matrix Links)
        for (auto& c : connections)
        {
            const auto& src = nodes[(size_t)c.source];
            const auto& dst = nodes[(size_t)c.target];
            
            // Gain health if nodes are active (Growth)
            if (src.energy > 0.05f || dst.energy > 0.05f)
                c.health = juce::jmin(1.0f, c.health + 0.002f * (src.energy + dst.energy));
            
            // Constant slow decay (Aging)
            if (!c.isPersistent)
                c.health = juce::jmax(0.0f, c.health - 0.0003f);
        }

        // Prune dead matrix links
        connections.erase(std::remove_if(connections.begin(), connections.end(),
            [](const NodeConnection& c) { return !c.isPersistent && c.health < 0.04f; }),
            connections.end());

        updateParticles(rms, 0.016f, beatPhase);
    }

    void updateParticles(float rms, float dt, float beatPhase)
    {
        for (auto& c : connections)
        {
            float absAmt  = std::abs(c.amount);
            bool inverted = (c.amount < 0.0f);

            // Dynamically resize particle count
            int targetCount = 2 + int(absAmt * 10.0f);
            while (int(c.particles.size()) < targetCount)
            {
                FlowParticle p;
                p.t     = juce::Random::getSystemRandom().nextFloat();
                p.speed = 0.1f + juce::Random::getSystemRandom().nextFloat() * 0.3f;
                c.particles.push_back(p);
            }
            if (int(c.particles.size()) > targetCount)
                c.particles.resize(size_t(targetCount));

            // Beat-synced speed modulation
            float bPulse = std::pow(std::sin(beatPhase * juce::MathConstants<float>::pi) * 0.5f + 0.5f, 2.0f);
            float beatSpeed = 0.5f + bPulse * 1.5f;

            for (auto& p : c.particles)
            {
                if (p.history.empty()) p.history.resize(12, p.t); 

                float step = p.speed * beatSpeed * (0.5f + absAmt * 2.0f + rms * 2.0f) * dt;

                if (inverted)
                {
                    p.t -= step;
                    if (p.t < 0.0f) p.t += 1.0f;
                }
                else
                {
                    p.t += step;
                    if (p.t > 1.0f) p.t -= 1.0f;
                }

                // Trail Move
                for (int i = (int)p.history.size() - 1; i > 0; --i)
                    p.history[(size_t)i] = p.history[(size_t)i - 1];

                p.history[0] = p.t;
            }
        }
    }

    void addConnection(int source, int target)
    {
        if (source < 0 || target < 0 || source == target) return;

        const juce::ScopedLock sl (lock);
        for (auto& c : connections)
        {
            if (c.source == source && c.target == target)
            {
                if (juce::isPositiveAndBelow(target, (int) nodes.size()))
                    c.targetSlot = nodes[(size_t) target].slotIndex;
                c.amount *= -1.0f; // Toggle inversion
                return;
            }
        }

        NodeConnection c;
        c.source = source;
        c.target = target;
        c.targetSlot = juce::isPositiveAndBelow(target, (int) nodes.size())
            ? nodes[(size_t) target].slotIndex
            : -1;
        c.amount = 0.5f;

        for (int i = 0; i < 4; ++i)
        {
            FlowParticle p;
            p.t     = juce::Random::getSystemRandom().nextFloat();
            p.speed = 0.1f + juce::Random::getSystemRandom().nextFloat() * 0.3f;
            c.particles.push_back(p);
        }

        connections.push_back(c);
    }

    void removeConnection(int source, int target)
    {
        const juce::ScopedLock sl (lock);
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [&](const NodeConnection& c) { return c.source == source && c.target == target; }),
            connections.end());
    }

    NodeSystemSnapshot createSnapshot() const
    {
        const juce::ScopedLock sl (lock);
        return { nodes, connections };
    }

    juce::ValueTree saveConnections() const
    {
        const juce::ScopedLock sl (lock);
        juce::ValueTree vt("CONNECTIONS");
        for (const auto& c : connections)
        {
            juce::ValueTree cv("LINK");
            cv.setProperty("src", c.source, nullptr);
            cv.setProperty("dst", c.target, nullptr);
            cv.setProperty("amt", c.amount, nullptr);
            cv.setProperty("h", c.health, nullptr);
            cv.setProperty("p", c.isPersistent, nullptr);
            vt.addChild(cv, -1, nullptr);
        }
        return vt;
    }

    void loadConnections(const juce::ValueTree& vt)
    {
        if (!vt.hasType("CONNECTIONS")) return;
        const juce::ScopedLock sl (lock);
        connections.clear();
        for (int i = 0; i < vt.getNumChildren(); ++i)
        {
            auto cv = vt.getChild(i);
            NodeConnection c;
            c.source = cv.getProperty("src");
            c.target = cv.getProperty("dst");
            c.targetSlot = juce::isPositiveAndBelow(c.target, (int) nodes.size())
                ? nodes[(size_t) c.target].slotIndex
                : -1;
            c.amount = cv.getProperty("amt", 0.5f);
            c.health = cv.getProperty("h", 1.0f);
            c.isPersistent = cv.getProperty("p", false);
            
            // Re-initialise particles
            for (int pIdx = 0; pIdx < 4; ++pIdx)
            {
                FlowParticle p;
                p.t = juce::Random::getSystemRandom().nextFloat();
                p.speed = 0.1f + juce::Random::getSystemRandom().nextFloat() * 0.3f;
                c.particles.push_back(p);
            }
            connections.push_back(c);
        }
    }

    std::vector<FlowNode>&       getNodes()       { return nodes; }
    const std::vector<FlowNode>& getNodes() const  { return nodes; }

    std::vector<NodeConnection>&       getConnections()       { return connections; }
    const std::vector<NodeConnection>& getConnections() const  { return connections; }

    juce::CriticalSection& getLock() const { return lock; }

private:
    mutable juce::CriticalSection lock;

    std::vector<FlowNode>      nodes;
    std::vector<NodeConnection> connections;
};
