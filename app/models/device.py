import uuid
from datetime import datetime
from sqlalchemy import String, Float, DateTime, Text, Enum as SAEnum
from sqlalchemy.orm import Mapped, mapped_column, relationship
from app.models.base import Base
import enum


class DeviceType(str, enum.Enum):
    SENSOR = "sensor"
    VALVE = "valve"
    PUMP = "pump"
    GATEWAY = "gateway"


class DeviceStatus(str, enum.Enum):
    ACTIVE = "active"
    INACTIVE = "inactive"
    OFFLINE = "offline"
    FAULT = "fault"


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    uid: Mapped[str] = mapped_column(
        String(64), unique=True, nullable=False, index=True,
        default=lambda: str(uuid.uuid4())
    )
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    device_type: Mapped[DeviceType] = mapped_column(
        SAEnum(DeviceType, length=16), nullable=False
    )
    status: Mapped[DeviceStatus] = mapped_column(
        SAEnum(DeviceStatus, length=16), default=DeviceStatus.INACTIVE
    )
    firmware_version: Mapped[str] = mapped_column(String(32), nullable=True)
    location: Mapped[str] = mapped_column(String(256), nullable=True)
    metadata: Mapped[str] = mapped_column(Text, nullable=True)
    last_seen: Mapped[datetime] = mapped_column(DateTime, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime, default=datetime.utcnow, onupdate=datetime.utcnow
    )

    readings = relationship("Reading", back_populates="device", lazy="selectin")
    commands = relationship("ValveCommand", back_populates="device", lazy="selectin")
