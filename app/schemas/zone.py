from datetime import datetime
from pydantic import BaseModel
from typing import Optional
from app.models.zone import ZoneStatus


class ZoneCreate(BaseModel):
    name: str
    description: Optional[str] = None
    area_sqm: Optional[float] = None
    valve_device_uid: Optional[str] = None
    order: int = 0


class ZoneUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None
    area_sqm: Optional[float] = None
    status: Optional[ZoneStatus] = None
    order: Optional[int] = None


class ZoneResponse(BaseModel):
    id: int
    name: str
    description: Optional[str] = None
    area_sqm: Optional[float] = None
    status: ZoneStatus
    valve_device_id: Optional[int] = None
    order: int
    created_at: datetime
    updated_at: datetime

    model_config = {"from_attributes": True}
