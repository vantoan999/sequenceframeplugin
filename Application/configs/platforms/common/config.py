dependencies = ["kzjs", "kzui"]

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

m.include_paths += [os.path.join("..", "..", "..", "src", "plugin", "src")]
m.include_paths += [os.path.join("..", "..", "..", "src", "plugin", "lz4")]
m.include_paths += [os.path.join("..", "..", "..", "src", "plugin", "zlib")]

del m

m = module(project_name)
m.depends += ["SequenceFramePlugin_component"]

if build_from_sources:
    m.depends += dependencies

else:
    m.used_libraries += dependencies

m.include_paths += [os.path.join("..", "..", "..", "src", "plugin", "src")]
m.include_paths += [os.path.join("..", "..", "..", "src", "plugin", "lz4")]
m.include_paths += [os.path.join("..", "..", "..", "src", "plugin", "zlib")]

m.root = os.path.join("..", "..", "..", "src", "executable")
m.output_path = os.path.join("..", "..", "..", "output", platform_name, profile_string, "executable")

m.used_libraries += ["v8"]
m.env["CPPDEFINES"] += ["KANZI_V8_API="]
m.env["CPPDEFINES"] += ["SEQUENCEFRAMEPLUGIN_API="]

del m
