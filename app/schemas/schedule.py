from datetime import datetime, time
from pydantic import BaseModel
from typing import Optional
from app.models.schedule import ScheduleType, ScheduleStatus


class ScheduleCreate(BaseModel):
    zone_id: int
    schedule_type: ScheduleType
    start_time: Optional[time] = None
    end_time: Optional[time] = None
    interval_minutes: Optional[int] = None
    duration_seconds: int
    moisture_threshold: Optional[float] = None


class ScheduleUpdate(BaseModel):
    status: Optional[ScheduleStatus] = None
    start_time: Optional[time] = None
    end_time: Optional[time] = None
    interval_minutes: Optional[int] = None
    duration_seconds: Optional[int] = None
    moisture_threshold: Optional[float] = None
    enabled: Optional[bool] = None


class ScheduleResponse(BaseModel):
    id: int
    zone_id: int
    schedule_type: ScheduleType
    status: ScheduleStatus
    start_time: Optional[time] = None
    end_time: Optional[time] = None
    interval_minutes: Optional[int] = None
    duration_seconds: int
    moisture_threshold: Optional[float] = None
    enabled: bool
    created_at: datetime
    updated_at: datetime

    model_config = {"from_attributes": True}
