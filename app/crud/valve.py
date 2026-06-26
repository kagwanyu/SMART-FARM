from datetime import datetime
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession
from app.models.valve import ValveCommand, CommandStatus
from app.schemas.valve import ValveCommandCreate


class ValveCRUD:
    @staticmethod
    async def create_command(
        db: AsyncSession, data: ValveCommandCreate, device_id: int
    ) -> ValveCommand:
        cmd = ValveCommand(
            device_id=device_id,
            command=data.command,
            position=data.position,
            issued_by=data.issued_by,
        )
        db.add(cmd)
        await db.commit()
        await db.refresh(cmd)
        return cmd

    @staticmethod
    async def get_pending_by_device(
        db: AsyncSession, device_id: int
    ) -> list[ValveCommand]:
        result = await db.execute(
            select(ValveCommand)
            .where(
                ValveCommand.device_id == device_id,
                ValveCommand.status == CommandStatus.PENDING,
            )
            .order_by(ValveCommand.created_at)
        )
        return list(result.scalars().all())

    @staticmethod
    async def mark_sent(db: AsyncSession, command_id: int) -> ValveCommand | None:
        result = await db.execute(
            select(ValveCommand).where(ValveCommand.id == command_id)
        )
        cmd = result.scalar_one_or_none()
        if cmd:
            cmd.status = CommandStatus.SENT
            cmd.sent_at = datetime.utcnow()
            await db.commit()
            await db.refresh(cmd)
        return cmd

    @staticmethod
    async def mark_acknowledged(
        db: AsyncSession, command_id: int
    ) -> ValveCommand | None:
        result = await db.execute(
            select(ValveCommand).where(ValveCommand.id == command_id)
        )
        cmd = result.scalar_one_or_none()
        if cmd:
            cmd.status = CommandStatus.ACKNOWLEDGED
            cmd.acknowledged_at = datetime.utcnow()
            await db.commit()
            await db.refresh(cmd)
        return cmd

    @staticmethod
    async def get_history(
        db: AsyncSession, device_id: int, limit: int = 50
    ) -> list[ValveCommand]:
        result = await db.execute(
            select(ValveCommand)
            .where(ValveCommand.device_id == device_id)
            .order_by(desc(ValveCommand.created_at))
            .limit(limit)
        )
        return list(result.scalars().all())


valve_crud = ValveCRUD()
