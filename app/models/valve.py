from datetime import datetime
from sqlalchemy import Integer, String, DateTime, ForeignKey, Enum as SAEnum
from sqlalchemy.orm import Mapped, mapped_column, relationship
from app.models.base import Base
import enum


class CommandType(str, enum.Enum):
    OPEN = "open"
    CLOSE = "close"
    SET_POSITION = "set_position"


class CommandStatus(str, enum.Enum):
    PENDING = "pending"
    SENT = "sent"
    ACKNOWLEDGED = "acknowledged"
    FAILED = "failed"


class ValveCommand(Base):
    __tablename__ = "valve_commands"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[int] = mapped_column(
        Integer, ForeignKey("devices.id"), nullable=False, index=True
    )
    zone_id: Mapped[int] = mapped_column(
        Integer, ForeignKey("zones.id"), nullable=True
    )
    command: Mapped[CommandType] = mapped_column(
        SAEnum(CommandType, length=16), nullable=False
    )
    position: Mapped[int] = mapped_column(Integer, nullable=True)
    status: Mapped[CommandStatus] = mapped_column(
        SAEnum(CommandStatus, length=16), default=CommandStatus.PENDING
    )
    issued_by: Mapped[str] = mapped_column(String(64), nullable=True)
    sent_at: Mapped[datetime] = mapped_column(DateTime, nullable=True)
    acknowledged_at: Mapped[datetime] = mapped_column(DateTime, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)

    device = relationship("Device", back_populates="commands")
