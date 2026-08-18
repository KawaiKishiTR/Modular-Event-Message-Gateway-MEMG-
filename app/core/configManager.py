from dataclasses import asdict, is_dataclass, fields
from pathlib import Path
from typing import Any, Dict, Type, TypeVar
from ruamel.yaml import YAML


T = TypeVar("T")

def _deserialize_dataclass(cls: Type[T], data: Dict[str, Any]) -> T | dict:
    """İç içe (Nested) dataclass yapılarını özyinelemeli (Recursive) olarak eşler."""
    if not is_dataclass(cls):
        return data
    field_types = {f.name: f.type for f in fields(cls)}
    init_kwargs = {}
    for name, value in data.items():
        if name in field_types:
            ftype = field_types[name]
            if is_dataclass(ftype) and isinstance(value, dict):
                init_kwargs[name] = _deserialize_dataclass(ftype, value)
            else:
                init_kwargs[name] = value
    return cls(**init_kwargs)

class BaseConfigModel:
    """C++ NLOHMANN makroları benzeri yükleme ve kaydetme sağlayan temel sınıf."""

    @classmethod
    def from_yaml(cls: Type[T], file_path: str | Path) -> T:
        yaml = YAML()
        yaml.preserve_quotes = True
        with open(file_path, "r", encoding="utf-8") as f:
            data = yaml.load(f) or {}
        return _deserialize_dataclass(cls, data)

    def to_dict(self) -> dict:
        return asdict(self)
        
    def to_yaml(self, file_path: str | Path) -> None:
        yaml = YAML()
        yaml.indent(mapping=2, sequence=4, offset=2)
        with open(file_path, "w", encoding="utf-8") as f:
            yaml.dump(self.to_dict(), f)