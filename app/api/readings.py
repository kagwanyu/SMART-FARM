from datetime import datetime
from fastapi import APIRouter, HTTPException, Query, status
from app.crud.reading import reading_crud
from app.crud.device import device_crud
from app.models.reading import Reading
from app.schemas.reading import ReadingCreate, ReadingResponse, ReadingBatchCreate
from app.core.deps import SessionDep

router = APIRouter(prefix="/readings", tags=["Readings"])


@router.post("/", response_model=ReadingResponse, status_code=status.HTTP_201_CREATED)
async def create_reading(data: ReadingCreate, db: SessionDep):
    device = await device_crud.get_by_uid(db, data.device_uid)
    if not device:
        raise HTTPException(status_code=404, detail=f"Device {data.device_uid} not found")
    reading = Reading(
        device_id=device.id,
        metric=data.metric,
        value=data.value,
        unit=data.unit,
        timestamp=data.timestamp or datetime.utcnow(),
    )
    db.add(reading)
    await db.commit()
    await db.refresh(reading)
    return reading


@router.post("/batch", status_code=status.HTTP_201_CREATED)
async def bulk_create_readings(data: ReadingBatchCreate, db: SessionDep):
    readings = []
    for r in data.readings:
        device = await device_crud.get_by_uid(db, r.device_uid)
        if not device:
            raise HTTPException(
                status_code=404, detail=f"Device {r.device_uid} not found"
            )
        readings.append(
            Reading(
                device_id=device.id,
                metric=r.metric,
                value=r.value,
                unit=r.unit,
                timestamp=r.timestamp or datetime.utcnow(),
            )
        )
    await reading_crud.bulk_create(db, readings)
    return {"created": len(readings)}


@router.get("/", response_model=list[ReadingResponse])
async def query_readings(
    db: SessionDep,
    device_uid: str | None = None,
    metric: str | None = None,
    from_: datetime | None = Query(None, alias="from"),
    to: datetime | None = None,
    limit: int = 100,
    offset: int = 0,
):
    return await reading_crud.query(db, device_uid, metric, from_, to, limit, offset)
