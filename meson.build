# , 'b_lundef=false', 'b_sanitize=address,undefined'
project('maidenless', 'c', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=gnu11', 'cpp_std=c++17', 'b_lto=true'])

if get_option('buildtype') == 'custom'
  add_global_arguments('-Ofast', '-g3', language : 'c')
  add_global_arguments('-Ofast', '-g3', language : 'cpp')
endif

add_global_arguments(
  '-Wall',
  '-Wno-pedantic',
  '-Wno-unused-parameter',
  '-Wno-nullability-extension',
  language: 'c',
)

# CPP files aren't my code, don't want to mess with the warnings
add_global_arguments(
  '-w',
  '-Wno-nullability-extension',
  language: 'cpp',
)

cc = meson.get_compiler('c')
m = cc.find_library('m', required : false)
dl = cc.find_library('dl', required : false)

sdl2 = dependency('SDL2')
opengl = dependency('GL')
freetype = dependency('freetype2')

if build_machine.system() == 'darwin'
    molten_dirs = []
    if import('fs').exists('/opt/homebrew/lib/libMoltenVK.dylib')
        molten_dirs += '/opt/homebrew/lib'
    endif
    vulkan = cc.find_library('MoltenVK', required : true, dirs : molten_dirs)
else
    vulkan = dependency('vulkan')
endif

glsllang = find_program('glslangValidator')

# vcc = find_program('vcc')

# chad_shader_targets = []

# chad_shaders = [
#   'source/shaders/immediate.vert.c',
#   'source/shaders/immediate.frag.c',
#   # 'source/shaders/text.frag.c',
# ]

# foreach s : chad_shaders
#   target_name = '@0@.spv'.format(s).split('/')
#   chad_shaders += custom_target(
#     target_name.get(target_name.length()-1),
#     command : [vcc, '--entry-point', 'main', '--no-dynamic-scheduling', '--execution-model', 'Fragment', '@INPUT@', '-o', '@OUTPUT@'],
#     build_by_default: true,
#     input : s, 
#     output : '@BASENAME@.spv'
#   )
# endforeach

# chad_shaders = declare_dependency(
#   sources: chad_shader_targets
# )

shader_targets = []

shaders = [
  'source/shaders/gbuffer.vert.glsl',
  'source/shaders/gbuffer.frag.glsl',
  'source/shaders/text.vert.glsl',
  'source/shaders/text.frag.glsl',
  'source/shaders/immediate.vert.glsl',
  'source/shaders/immediate.frag.glsl',
  # 'source/shaders/shading.comp.glsl',
  'source/shaders/systems/model_matrix_frustum.comp.glsl',
  # 'source/shaders/systems/path_finding.comp.glsl',
]

foreach s : shaders
  target_name = '@0@.spv'.format(s).split('/')
  shaders += custom_target(
    target_name.get(target_name.length()-1),
    command : [glsllang, '-V', '@INPUT@', '-o', '@OUTPUT@'],
    build_by_default: true,
    input : s, 
    output : '@BASENAME@.spv'
  )
endforeach

shaders = declare_dependency(
  sources: shader_targets
)

image_to_c = executable('image_to_c', 'external/image_to_c.c')

images = ['source/resources/no_image.png']

image_targets = []

foreach i : images
  target_name = '@0@.png'.format(s).split('/')
  image_targets += custom_target(
      target_name.get(target_name.length()-1),
      output : '@BASENAME@.c',
      input : i,
      command : [image_to_c, '@INPUT@'],
      capture : true # write stdout content to output file
  )
endforeach

images = declare_dependency(
  sources: image_targets
)

jps = executable('a_star',
  'benchmark/jps.c',
  'external/jps.c',
  'external/heap.c',
  'external/intlist.c',
  include_directories: [include_directories('external/')]
)

imgui = static_library('imgui',
  'external/cimgui.cpp',
  'external/imgui/imgui_draw.cpp',
  'external/imgui/imgui_impl_sdl2.cpp',
  'external/imgui/imgui_impl_vulkan.cpp',
  'external/imgui/imgui_tables.cpp',
  'external/imgui/imgui_demo.cpp',
  'external/imgui/imgui.cpp',
  'external/imgui/imgui_widgets.cpp',
  include_directories: [include_directories('external/')]
)

qcvm = static_library('qcvm',
  'external/qcvm/qcvm_bootstrap.c',
  'external/qcvm/qcvm_entities.c',
  'external/qcvm/qcvm_exports.c',
  'external/qcvm/qcvm_fields.c',
  'external/qcvm/qcvm_functions.c',
  'external/qcvm/qcvm_globals.c',
  'external/qcvm/qcvm_opcodes.c',
  'external/qcvm/qcvm_opcodes.h',
  'external/qcvm/qcvm_parameters.c',
  'external/qcvm/qcvm_return.c',
  'external/qcvm/qcvm_runtime.c',
  'external/qclib/qclib.c',
  include_directories: [include_directories('external/qcvm/'), include_directories('external/qclib/')]
)

exe = executable('maidenless',
  'source/maidenless.c',

  'source/client/cl_client.c',
  'source/client/cl_console.c',
  'source/client/cl_imgui.cpp',

  'source/game/g_game.c',
  'source/game/g_bindings.c',
  'source/game/g_data.c',
  'source/game/g_terrain.c',
  'source/game/g_ui.c',
  'source/game/g_localization.c',

  'source/vk/vk.c',
  'source/vk/vk_gbuffer.c',
  'source/vk/vk_immediate.c',
  'source/vk/vk_system.c',

  'source/common/c_job.c',
  'source/common/c_profiler.c',

  'external/zpl/zpl_implementation.c',
  'external/vk_mem_alloc.cpp',
  'external/stbi_image.c',
  'external/stbi_image_write.c',
  'external/minini/minIni.c',

  'external/jps.c',
  'external/heap.c',
  'external/intlist.c',

  install : true,
  include_directories: [
    include_directories('source/'),
    include_directories('external/'),
    include_directories('external/qcvm/'),
    include_directories('external/qclib/')
  ],
  link_with: [qcvm, imgui],
  dependencies: [sdl2, opengl, vulkan, freetype, dl, m, shaders, images]
)
