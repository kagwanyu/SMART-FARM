from datetime import datetime, time
from sqlalchemy import Integer, String, Float, Boolean, Time, DateTime, ForeignKey, Enum as SAEnum
from sqlalchemy.orm import Mapped, mapped_column
from app.models.base import Base
import enum


class ScheduleType(str, enum.Enum):
    INTERVAL = "interval"
    TIMED = "timed"
    MOISTURE_TRIGGERED = "moisture_triggered"


class ScheduleStatus(str, enum.Enum):
    ACTIVE = "active"
    PAUSED = "paused"
    COMPLETED = "completed"


class Schedule(Base):
    __tablename__ = "schedules"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    zone_id: Mapped[int] = mapped_column(
        Integer, ForeignKey("zones.id"), nullable=False, index=True
    )
    schedule_type: Mapped[ScheduleType] = mapped_column(
        SAEnum(ScheduleType, length=32), nullable=False
    )
    status: Mapped[ScheduleStatus] = mapped_column(
        SAEnum(ScheduleStatus, length=16), default=ScheduleStatus.ACTIVE
    )
    start_time: Mapped[time] = mapped_column(Time, nullable=True)
    end_time: Mapped[time] = mapped_column(Time, nullable=True)
    interval_minutes: Mapped[int] = mapped_column(Integer, nullable=True)
    duration_seconds: Mapped[int] = mapped_column(Integer, nullable=False)
    moisture_threshold: Mapped[float] = mapped_column(Float, nullable=True)
    enabled: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime, default=datetime.utcnow, onupdate=datetime.utcnow
    )
