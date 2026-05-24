---
name: tab5template-example-apps
description: Use for creating or fixing standalone example applications under Examples/, including correct ESP-IDF project layout, CMake paths, and component wiring for Tab5Template.
---

# Tab5Template Example App Construction

Guidance for creating buildable, standalone ESP-IDF examples inside Examples/.

## When To Use This Skill

Use this skill when a request asks to:
- Create a new app in Examples/.
- Copy or adapt main application logic into an example.
- Fix CMake path errors for include or component directories.
- Make Example apps build independently from their own folder.

## Required Example Project Layout

For Examples/<AppName>/ use:
- CMakeLists.txt (root project CMake)
- main/CMakeLists.txt (main component registration)
- main/<AppName>.cpp (app entry code)
- partitions.csv symlink to ../../partitions.csv
- sdkconfig.defaults symlink to ../../sdkconfig.defaults

Optional generated files (do not handcraft):
- build/
- managed_components/
- sdkconfig
- dependencies.lock

## Canonical Root CMake Pattern

The root CMakeLists.txt must:
1. Set C and C++ standards.
2. Append extra component dirs:
   - ${CMAKE_CURRENT_SOURCE_DIR}/../../../components/Tab5 for deeply nested examples like IMU/01-BasicReadings
   - ${CMAKE_CURRENT_SOURCE_DIR}/../../components/Tab5 for one-level examples like Examples/Drawing
3. Append managed components with matching depth.
4. Set IDF_TARGET to esp32p4.
5. Point SDKCONFIG_DEFAULTS at repository sdkconfig.defaults with matching depth.
6. Include project.cmake then call project(<Name>).
7. Apply compile option suppression used by repository CMake files.

## Canonical main/CMakeLists.txt Pattern

Use idf_component_register with:
- SRCS "<AppName>.cpp"
- INCLUDE_DIRS path to repository components/Tab5 matching depth from main/.
- REQUIRES Tab5 plus any directly used IDF components.

Depth rule for INCLUDE_DIRS in main/CMakeLists.txt:
- For Examples/IMU/01-BasicReadings/main: ../../../../components/Tab5
- For Examples/Drawing/main and Examples/WiFiScanner/main: ../../../components/Tab5

## Common Failure Signature And Fix

Failure:
- Include directory .../Examples/components/Tab5 is not a directory.

Cause:
- Relative path depth is one level short.

Fix:
- Recalculate from folder containing the active CMakeLists.txt, not from repo root.
- Correct ../ depth in both root CMakeLists and main/CMakeLists.

## Splash And Identity Rules

When cloning app code into a new example:
- Update LOG_TAG to app-specific name.
- Update splash title string to app-specific title.
- Keep hardware initialisation order intact unless the example intentionally narrows scope.

## Productive Prompt Patterns

Use queries like:
- "Create Examples/<AppName> as a standalone ESP-IDF app matching IMU example structure."
- "Fix include path depth in main/CMakeLists for this example and explain the relative path math."
- "Clone main app into a new example and rename splash text, file names, and project() name consistently."
- "Audit all Examples/* CMakeLists files for incorrect component path depth."
