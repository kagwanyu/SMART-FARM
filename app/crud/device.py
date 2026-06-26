from datetime import datetime
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models.device import Device, DeviceStatus
from app.schemas.device import DeviceCreate, DeviceUpdate


class DeviceCRUD:
    @staticmethod
    async def create(db: AsyncSession, data: DeviceCreate) -> Device:
        device = Device(**data.model_dump(exclude_none=True))
        db.add(device)
        await db.commit()
        await db.refresh(device)
        return device

    @staticmethod
    async def get_by_uid(db: AsyncSession, uid: str) -> Device | None:
        result = await db.execute(select(Device).where(Device.uid == uid))
        return result.scalar_one_or_none()

    @staticmethod
    async def get_by_id(db: AsyncSession, device_id: int) -> Device | None:
        result = await db.execute(select(Device).where(Device.id == device_id))
        return result.scalar_one_or_none()

    @staticmethod
    async def get_all(
        db: AsyncSession, skip: int = 0, limit: int = 100
    ) -> list[Device]:
        result = await db.execute(
            select(Device).offset(skip).limit(limit).order_by(Device.id)
        )
        return list(result.scalars().all())

    @staticmethod
    async def update(
        db: AsyncSession, device: Device, data: DeviceUpdate
    ) -> Device:
        for key, value in data.model_dump(exclude_none=True).items():
            setattr(device, key, value)
        device.updated_at = datetime.utcnow()
        await db.commit()
        await db.refresh(device)
        return device

    @staticmethod
    async def heartbeat(db: AsyncSession, uid: str) -> Device | None:
        device = await DeviceCRUD.get_by_uid(db, uid)
        if device:
            device.last_seen = datetime.utcnow()
            device.status = DeviceStatus.ACTIVE
            await db.commit()
            await db.refresh(device)
        return device

    @staticmethod
    async def delete(db: AsyncSession, device: Device) -> None:
        await db.delete(device)
        await db.commit()


device_crud = DeviceCRUD()
