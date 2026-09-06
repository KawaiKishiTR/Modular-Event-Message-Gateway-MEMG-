import app.core
from app.main import main
from app.core.MainBus import MainBus

MainBus().subscribe(MainBus.Events.START_APP, main)
