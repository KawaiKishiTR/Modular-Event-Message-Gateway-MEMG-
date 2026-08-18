from dataclasses import dataclass, field

from app.core.configManager import BaseConfigModel


@dataclass
class NetworkConfig(BaseConfigModel):
    host:str = "127.0.0.1"
    port:int = 8080

@dataclass
class AppConfig(BaseConfigModel):
    mode:str = "client"
    network:NetworkConfig = field(default_factory=NetworkConfig)


