import asyncio
from app.core.MainBus import MainBus

async def run_app():
    # Sinyali ateşle
    await MainBus().emit(MainBus.Events.START_APP)
    
    # Uygulama kapatılana (Ctrl+C vb.) kadar loop'u ayakta tut:
    stop_event = asyncio.Event()
    try:
        await stop_event.wait()
    except (KeyboardInterrupt, asyncio.CancelledError):
        print("\nClosing app...")
        await MainBus().emit(MainBus.Events.STOP_APP)

def main():
    asyncio.run(run_app())


if __name__ == "__main__":
    main()

