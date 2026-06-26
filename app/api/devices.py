from fastapi import APIRouter, HTTPException, status
from app.crud.device import device_crud
from app.crud.reading import reading_crud
from app.schemas.device import DeviceCreate, DeviceUpdate, DeviceResponse, DeviceHeartbeat
from app.schemas.reading import ReadingResponse
from app.core.deps import SessionDep

router = APIRouter(prefix="/devices", tags=["Devices"])


@router.post("/", response_model=DeviceResponse, status_code=status.HTTP_201_CREATED)
async def register_device(data: DeviceCreate, db: SessionDep):
    return await device_crud.create(db, data)


@router.get("/", response_model=list[DeviceResponse])
async def list_devices(
    db: SessionDep, skip: int = 0, limit: int = 100
):
    return await device_crud.get_all(db, skip, limit)


@router.get("/{uid}", response_model=DeviceResponse)
async def get_device(uid: str, db: SessionDep):
    device = await device_crud.get_by_uid(db, uid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return device


@router.patch("/{uid}", response_model=DeviceResponse)
async def update_device(uid: str, data: DeviceUpdate, db: SessionDep):
    device = await device_crud.get_by_uid(db, uid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return await device_crud.update(db, device, data)


@router.post("/heartbeat", response_model=DeviceResponse)
async def device_heartbeat(data: DeviceHeartbeat, db: SessionDep):
    device = await device_crud.heartbeat(db, data.uid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return device


@router.delete("/{uid}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_device(uid: str, db: SessionDep):
    device = await device_crud.get_by_uid(db, uid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    await device_crud.delete(db, device)


@router.get("/{uid}/readings", response_model=list[ReadingResponse])
async def get_device_readings(
    uid: str,
    db: SessionDep,
    metric: str | None = None,
    limit: int = 100,
):
    return await reading_crud.query(db, device_uid=uid, metric=metric, limit=limit)
