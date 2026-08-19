import os
import asyncio
from time import sleep
from dotenv import load_dotenv

from app.core import MainBus
from app.core.configModels import AppConfig



def main():
    load_dotenv()
    load_env_variables()

    global APP_CONF
    APP_CONF = AppConfig.from_yaml("config\\config.yaml")

    print_appinfo()
    asyncio.run(MainBus().emit(MainBus.Events.START_APP))

def load_env_variables():
    global APP_NAME;        APP_NAME        = os.getenv("APP_NAME") 
    global APP_VERSION;     APP_VERSION     = os.getenv("APP_VERSION")
    global CODENAME;        CODENAME        = os.getenv("CODENAME")
    global RELEASE_DATE;    RELEASE_DATE    = os.getenv("RELEASE_DATE")

    global APP_CONTEXT;     APP_CONTEXT = {
        "APP_NAME":APP_NAME,
        "APP_VERSION":APP_VERSION,
        "CODENAME":CODENAME,
        "RELEASE_DATE":RELEASE_DATE
    }


def print_appinfo():
    justify = lambda str_, width : str_.ljust(width)
    line = lambda:print("="*(just_width+val_width+2))

    just_width = max([len(x) for x in APP_CONTEXT.keys()])+1
    val_width = max([len(str(x)) for x in APP_CONTEXT.values()])+1

    print()
    line()
    for k, v in APP_CONTEXT.items():
        print(f"{justify(k, just_width)} : {v}")


    line()
    print(f"\nRunning at: {APP_CONF.network.host}:{APP_CONF.network.port} on {APP_CONF.mode} mode\n")
    line()
    sleep(2)
    print()

if __name__ == "__main__":
    main()
