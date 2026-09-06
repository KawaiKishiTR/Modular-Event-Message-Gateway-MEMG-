from pathlib import Path
from typing import Callable
from dataclasses import dataclass, fields

from app.core.configModels import AppConfig
from app.core.container import Container

_ROOT_DIR:Path = Path(__file__).parent.parent
_CONFIG_FILE:Path = _ROOT_DIR / "config" / "config.yaml"
_JUST_LENGHT:int
_LINE_FUNC:Callable
_JUSTIFY_FUNC:Callable

@dataclass(frozen=True)
class APP_INFO:
    def __init__(self) -> None: # parametre verilmesini önlemek için
        super().__init__()      # __init__ fonksiyonu parametresiz override edilir

    APP_NAME:str        = "Modular Event Massage Gateway (MEMG)"
    APP_VERSION:str     = "0.1.0a (alpha)"
    CODENAME:str        = "Curious Squirrel"
    RELEASE_DATE:str    = "30.08.2026"

def calc_lenght(ctx:dict):
    global _JUSTIFY_FUNC
    global _JUST_LENGHT
    global _LINE_FUNC

    val_width = max([len(str(x)) for x in ctx.values()])+1
    _JUST_LENGHT = max([len(x) for x in ctx.keys()])+1
    _LINE_FUNC = lambda:print("="*(_JUST_LENGHT+val_width+2))
    _JUSTIFY_FUNC = lambda str_, width : str_.ljust(width)

def print_appinfo(ctx:dict):
    _LINE_FUNC()
    for k, v in ctx.items():
        print(f"{_JUSTIFY_FUNC(k, _JUST_LENGHT)} : {v}")

    _LINE_FUNC()

def load_config():
    Container().set(AppConfig, AppConfig.from_yaml(_CONFIG_FILE))
    CONF = Container().get(AppConfig)

    _LINE_FUNC()
    print(f"\nRunning at: {CONF.network.host}:{CONF.network.port} on {CONF.mode} mode\n")
    _LINE_FUNC()

def main():
    ctx = {fld.name:fld.default for fld in fields(APP_INFO)}

    calc_lenght(ctx)
    print_appinfo(ctx)
    load_config()