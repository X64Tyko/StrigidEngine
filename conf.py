#!/usr/bin/env python3
# m.css Doxygen configuration for TrinyxEngine
# Run: python vendor/mcss/documentation/doxygen.py conf.py
# Doxygen must be run first:  doxygen Doxyfile  (writes XML to docs/doxygen/xml/)

DOXYFILE = 'Doxyfile'
OUTPUT   = 'docs/doxygen/html'

MAIN_PROJECT_URL = '.'

THEME_COLOR = '#1a1025'

# CSS load order: m.css dark base → project accent overrides.
# Relative paths are copied into the output directory by m.css automatically.
STYLESHEETS = [
    'https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600'
    '&family=JetBrains+Mono:ital,wght@0,400;0,600;1,400&display=swap',
    'vendor/mcss/css/m-dark+documentation.compiled.css',
    'docs/theme/trinyx.css',
]

# Guides (narrative .md pages) on the left; API reference on the right.
LINKS_NAVBAR1 = [
    ('Guides', 'pages', []),
]
LINKS_NAVBAR2 = [
    ('Modules',    'modules',    []),
    ('Classes',    'annotated',  []),
    ('Namespaces', 'namespaces', []),
    ('Files',      'files',      []),
]

# m.code   — C++ syntax highlighting in .md pages
# m.components — callout boxes (@note, @warning, banner blocks)
# m.dox    — @ref cross-links from docs pages to API symbols
# m.vk     — auto-links VkDevice, VkCommandBuffer etc. to Vulkan spec
PLUGINS = ['m.code', 'm.components', 'm.dox', 'm.vk']

SHOW_UNDOCUMENTED = True

FINE_PRINT = (
    '<p>TrinyxEngine &mdash; data-oriented engine for competitive multiplayer.<br/>'
    'Generated from source. See the <a href="https://github.com/X64Tyko/TrinyxEngine">'
    'GitHub repository</a>.</p>'
)
