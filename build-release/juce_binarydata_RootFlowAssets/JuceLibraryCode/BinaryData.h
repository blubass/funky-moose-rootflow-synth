/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   wood_texture_png;
    const int            wood_texture_pngSize = 80514;

    extern const char*   metal_texture_png;
    const int            metal_texture_pngSize = 545293;

    extern const char*   roots_bg_png;
    const int            roots_bg_pngSize = 2196113;

    extern const char*   roots_overlay_png;
    const int            roots_overlay_pngSize = 795029;

    extern const char*   roots_overlay_soft_png;
    const int            roots_overlay_soft_pngSize = 2069150;

    extern const char*   roots_overlay_detail_png;
    const int            roots_overlay_detail_pngSize = 1791459;

    extern const char*   moose_logo_png;
    const int            moose_logo_pngSize = 494640;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 7;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
