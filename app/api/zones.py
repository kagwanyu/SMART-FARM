from fastapi import APIRouter, HTTPException, status
from app.crud.zone import zone_crud
from app.schemas.zone import ZoneCreate, ZoneUpdate, ZoneResponse
from app.core.deps import SessionDep

router = APIRouter(prefix="/zones", tags=["Zones"])


@router.post("/", response_model=ZoneResponse, status_code=status.HTTP_201_CREATED)
async def create_zone(data: ZoneCreate, db: SessionDep):
    return await zone_crud.create(db, data)


@router.get("/", response_model=list[ZoneResponse])
async def list_zones(db: SessionDep, skip: int = 0, limit: int = 100):
    return await zone_crud.get_all(db, skip, limit)


@router.get("/{zone_id}", response_model=ZoneResponse)
async def get_zone(zone_id: int, db: SessionDep):
    zone = await zone_crud.get_by_id(db, zone_id)
    if not zone:
        raise HTTPException(status_code=404, detail="Zone not found")
    return zone


@router.patch("/{zone_id}", response_model=ZoneResponse)
async def update_zone(zone_id: int, data: ZoneUpdate, db: SessionDep):
    zone = await zone_crud.get_by_id(db, zone_id)
    if not zone:
        raise HTTPException(status_code=404, detail="Zone not found")
    return await zone_crud.update(db, zone, data)


@router.delete("/{zone_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_zone(zone_id: int, db: SessionDep):
    zone = await zone_crud.get_by_id(db, zone_id)
    if not zone:
        raise HTTPException(status_code=404, detail="Zone not found")
    await zone_crud.delete(db, zone)
