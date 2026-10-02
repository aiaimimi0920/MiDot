def can_build(env, platform):
    return platform == "windows" and env["arch"] == "x86_64"


def configure(env):
    pass


def get_doc_classes():
    return ["Spout"]


def get_doc_path():
    return "doc_classes"
