from fastapi import APIRouter, HTTPException, status
from app.crud.schedule import schedule_crud
from app.schemas.schedule import ScheduleCreate, ScheduleUpdate, ScheduleResponse
from app.core.deps import SessionDep

router = APIRouter(prefix="/schedules", tags=["Schedules"])


@router.post("/", response_model=ScheduleResponse, status_code=status.HTTP_201_CREATED)
async def create_schedule(data: ScheduleCreate, db: SessionDep):
    return await schedule_crud.create(db, data)


@router.get("/", response_model=list[ScheduleResponse])
async def list_schedules(db: SessionDep, skip: int = 0, limit: int = 100):
    return await schedule_crud.get_all(db, skip, limit)


@router.get("/active", response_model=list[ScheduleResponse])
async def get_active_schedules(db: SessionDep):
    return await schedule_crud.get_active(db)


@router.get("/zone/{zone_id}", response_model=list[ScheduleResponse])
async def get_zone_schedules(zone_id: int, db: SessionDep):
    return await schedule_crud.get_by_zone(db, zone_id)


@router.get("/{schedule_id}", response_model=ScheduleResponse)
async def get_schedule(schedule_id: int, db: SessionDep):
    schedule = await schedule_crud.get_by_id(db, schedule_id)
    if not schedule:
        raise HTTPException(status_code=404, detail="Schedule not found")
    return schedule


@router.patch("/{schedule_id}", response_model=ScheduleResponse)
async def update_schedule(schedule_id: int, data: ScheduleUpdate, db: SessionDep):
    schedule = await schedule_crud.get_by_id(db, schedule_id)
    if not schedule:
        raise HTTPException(status_code=404, detail="Schedule not found")
    return await schedule_crud.update(db, schedule, data)


@router.delete("/{schedule_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_schedule(schedule_id: int, db: SessionDep):
    schedule = await schedule_crud.get_by_id(db, schedule_id)
    if not schedule:
        raise HTTPException(status_code=404, detail="Schedule not found")
    await schedule_crud.delete(db, schedule)
