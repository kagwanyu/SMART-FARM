from datetime import datetime
from sqlalchemy import String, Float, Integer, DateTime, ForeignKey, Enum as SAEnum
from sqlalchemy.orm import Mapped, mapped_column, relationship
from app.models.base import Base
import enum


class ZoneStatus(str, enum.Enum):
    IDLE = "idle"
    WATERING = "watering"
    COMPLETED = "completed"
    PAUSED = "paused"
    FAULT = "fault"


class Zone(Base):
    __tablename__ = "zones"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    description: Mapped[str] = mapped_column(String(512), nullable=True)
    area_sqm: Mapped[float] = mapped_column(Float, nullable=True)
    status: Mapped[ZoneStatus] = mapped_column(
        SAEnum(ZoneStatus, length=16), default=ZoneStatus.IDLE
    )
    valve_device_id: Mapped[int] = mapped_column(
        Integer, ForeignKey("devices.id"), nullable=True
    )
    order: Mapped[int] = mapped_column(Integer, default=0)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime, default=datetime.utcnow, onupdate=datetime.utcnow
    )

    valve = relationship("Device", foreign_keys=[valve_device_id])
