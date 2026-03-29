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
    float amount = 0.5f;

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

        const char* params[] = {
            "rootDepth", "rootSoil", "rootAnchor",
            "sapFlow", "sapVitality", "sapTexture",
            "pulseRate", "pulseBreath", "pulseGrowth",
            "canopy", "atmosphere", "instability", "ecoSystem",
            "bloom", "rain", "sun"
        };
        const int numParams = 16;

        for (int i = 0; i < numParams; ++i)
        {
            FlowNode n;
            n.paramID = params[i];
            n.param   = tree.getRawParameterValue(params[i]);
            if (n.param) n.value = n.param->load();
            n.position  = getDefaultNodePosition(i);

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
            
            float speed = 1.0f + n.value * 4.0f;
            n.activity  = 0.5f + 0.5f * std::sin(time * speed + float(i));

            // Organic Drift – nodes float like plankton (only when not dragged)
            if (!n.isDragging)
            {
                n.position.x = juce::jlimit(0.05f, 0.95f,
                    n.position.x + std::sin(time * 0.5f + (float)i * 1.3f) * 0.0004f);
                n.position.y = juce::jlimit(0.05f, 0.95f,
                    n.position.y + std::cos(time * 0.4f + (float)i * 1.1f) * 0.0004f);
            }
        }

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
                c.amount *= -1.0f; // Toggle inversion
                return;
            }
        }

        NodeConnection c;
        c.source = source;
        c.target = target;
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
