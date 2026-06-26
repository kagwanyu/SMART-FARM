from datetime import datetime
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models.schedule import Schedule, ScheduleStatus
from app.schemas.schedule import ScheduleCreate, ScheduleUpdate


class ScheduleCRUD:
    @staticmethod
    async def create(db: AsyncSession, data: ScheduleCreate) -> Schedule:
        schedule = Schedule(**data.model_dump())
        db.add(schedule)
        await db.commit()
        await db.refresh(schedule)
        return schedule

    @staticmethod
    async def get_by_id(db: AsyncSession, schedule_id: int) -> Schedule | None:
        result = await db.execute(select(Schedule).where(Schedule.id == schedule_id))
        return result.scalar_one_or_none()

    @staticmethod
    async def get_by_zone(
        db: AsyncSession, zone_id: int
    ) -> list[Schedule]:
        result = await db.execute(
            select(Schedule).where(Schedule.zone_id == zone_id)
        )
        return list(result.scalars().all())

    @staticmethod
    async def get_active(db: AsyncSession) -> list[Schedule]:
        result = await db.execute(
            select(Schedule).where(
                Schedule.status == ScheduleStatus.ACTIVE,
                Schedule.enabled == True,
            )
        )
        return list(result.scalars().all())

    @staticmethod
    async def get_all(
        db: AsyncSession, skip: int = 0, limit: int = 100
    ) -> list[Schedule]:
        result = await db.execute(
            select(Schedule).offset(skip).limit(limit).order_by(Schedule.id)
        )
        return list(result.scalars().all())

    @staticmethod
    async def update(
        db: AsyncSession, schedule: Schedule, data: ScheduleUpdate
    ) -> Schedule:
        for key, value in data.model_dump(exclude_none=True).items():
            setattr(schedule, key, value)
        schedule.updated_at = datetime.utcnow()
        await db.commit()
        await db.refresh(schedule)
        return schedule

    @staticmethod
    async def delete(db: AsyncSession, schedule: Schedule) -> None:
        await db.delete(schedule)
        await db.commit()


schedule_crud = ScheduleCRUD()
