from datetime import datetime
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession
from app.models.reading import Reading
from app.schemas.reading import ReadingCreate


class ReadingCRUD:
    @staticmethod
    async def create(db: AsyncSession, data: ReadingCreate) -> Reading:
        reading = Reading(
            device_id=data.device_uid,
            metric=data.metric,
            value=data.value,
            unit=data.unit,
            timestamp=data.timestamp or datetime.utcnow(),
        )
        db.add(reading)
        await db.commit()
        await db.refresh(reading)
        return reading

    @staticmethod
    async def bulk_create(db: AsyncSession, readings: list[Reading]) -> list[Reading]:
        db.add_all(readings)
        await db.commit()
        return readings

    @staticmethod
    async def query(
        db: AsyncSession,
        device_uid: str | None = None,
        metric: str | None = None,
        from_: datetime | None = None,
        to: datetime | None = None,
        limit: int = 100,
        offset: int = 0,
    ) -> list[Reading]:
        stmt = select(Reading)
        if device_uid:
            from app.models.device import Device
            subq = select(Device.id).where(Device.uid == device_uid).scalar_subquery()
            stmt = stmt.where(Reading.device_id.in_(subq))
        if metric:
            stmt = stmt.where(Reading.metric == metric)
        if from_:
            stmt = stmt.where(Reading.timestamp >= from_)
        if to:
            stmt = stmt.where(Reading.timestamp <= to)
        stmt = stmt.order_by(desc(Reading.timestamp)).offset(offset).limit(limit)
        result = await db.execute(stmt)
        return list(result.scalars().all())

    @staticmethod
    async def get_latest_by_metric(
        db: AsyncSession, device_id: int, metric: str
    ) -> Reading | None:
        result = await db.execute(
            select(Reading)
            .where(Reading.device_id == device_id, Reading.metric == metric)
            .order_by(desc(Reading.timestamp))
            .limit(1)
        )
        return result.scalar_one_or_none()


reading_crud = ReadingCRUD()
