import unreal


def log(label, value):
    unreal.log_warning(f"CODEX_{label} {value}")


log("UNREAL_WIDGET_TYPES", [name for name in dir(unreal) if "WidgetBlueprint" in name or "WidgetTree" in name])

for type_name in ("WidgetBlueprintEditorSubsystem", "WidgetBlueprintLibrary", "WidgetTree"):
    value = getattr(unreal, type_name, None)
    log(f"TYPE_{type_name}", value)
    if value:
        log(f"MEMBERS_{type_name}", [name for name in dir(value) if not name.startswith("_")])

asset = unreal.load_asset("/Game/Blueprints/Widget/WBP_BossHealthBar")
log("ASSET", asset)
if asset:
    log("ASSET_MEMBERS", [name for name in dir(asset) if not name.startswith("_")])
    generated_class = asset.generated_class()
    log("GENERATED_CLASS", generated_class)
    log("CLASS_MEMBERS", [name for name in dir(generated_class) if not name.startswith("_")])
    default_object = unreal.get_default_object(generated_class)
    log("DEFAULT_OBJECT", default_object)
    log("DEFAULT_MEMBERS", [name for name in dir(default_object) if "widget" in name.lower() or "tree" in name.lower()])
