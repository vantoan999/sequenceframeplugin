dependencies = ["kzui"]

m = module("SequenceFramePlugin_component")
m.type = "a"
if build_from_sources:
    m.depends += dependencies
    if GetOption("build-dynamic-libs"): # This option is set when --dynamic is used
        m.type = "so"
        m.env["SHLINKFLAGS"] += ["-shared"]
else:
    m.used_libraries += dependencies

m.root = os.path.join("..", "..", "..", "src", "plugin")
m.output_path = os.path.join("..", "..", "..", "output", platform_name, profile_string, "plugin")

m.env["CPPDEFINES"] += ["SEQUENCEFRAMEPLUGIN_API="]

del m

m = module(project_name)
m.depends += ["SequenceFramePlugin_component"]

if build_from_sources:
    m.depends += dependencies
else:
    m.used_libraries += dependencies

m.include_paths += [os.path.join("..", "..", "..", "src", "plugin", "src")]

m.root = os.path.join("..", "..", "..", "src", "executable")
m.output_path = os.path.join("..", "..", "..", "output", platform_name, profile_string, "executable")

m.env["CPPDEFINES"] += ["SEQUENCEFRAMEPLUGIN_API="]

# Linking explicitely all available Kanzi font engine backends to the application module.
# You can pass either "kzfreetypeplugin" or "kzitypeplugin" as second argument for the 
# link_font_engine_backends_to_module function to link particular font engine backend
# to your application module.
#
# In case you are working with dynamic libraries you can can remove the explicit linking
# and let Kanzi engine discover available font engine backends runtime.
# You can use FontEngine application configuration to choose preferred font engine in case
# there is many available.
link_font_engine_backends_to_module(m);

del m
