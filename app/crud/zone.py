from datetime import datetime
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models.zone import Zone
from app.schemas.zone import ZoneCreate, ZoneUpdate


class ZoneCRUD:
    @staticmethod
    async def create(db: AsyncSession, data: ZoneCreate) -> Zone:
        zone = Zone(**data.model_dump(exclude_none=True))
        db.add(zone)
        await db.commit()
        await db.refresh(zone)
        return zone

    @staticmethod
    async def get_by_id(db: AsyncSession, zone_id: int) -> Zone | None:
        result = await db.execute(select(Zone).where(Zone.id == zone_id))
        return result.scalar_one_or_none()

    @staticmethod
    async def get_all(db: AsyncSession, skip: int = 0, limit: int = 100) -> list[Zone]:
        result = await db.execute(
            select(Zone).offset(skip).limit(limit).order_by(Zone.order)
        )
        return list(result.scalars().all())

    @staticmethod
    async def update(db: AsyncSession, zone: Zone, data: ZoneUpdate) -> Zone:
        for key, value in data.model_dump(exclude_none=True).items():
            setattr(zone, key, value)
        zone.updated_at = datetime.utcnow()
        await db.commit()
        await db.refresh(zone)
        return zone

    @staticmethod
    async def delete(db: AsyncSession, zone: Zone) -> None:
        await db.delete(zone)
        await db.commit()


zone_crud = ZoneCRUD()
