from datetime import datetime
from pydantic import BaseModel, Field
from typing import Optional
from app.models.device import DeviceType, DeviceStatus


class DeviceCreate(BaseModel):
    uid: Optional[str] = None
    name: str
    device_type: DeviceType
    firmware_version: Optional[str] = None
    location: Optional[str] = None
    metadata: Optional[str] = None


class DeviceUpdate(BaseModel):
    name: Optional[str] = None
    status: Optional[DeviceStatus] = None
    firmware_version: Optional[str] = None
    location: Optional[str] = None
    metadata: Optional[str] = None


class DeviceResponse(BaseModel):
    id: int
    uid: str
    name: str
    device_type: DeviceType
    status: DeviceStatus
    firmware_version: Optional[str] = None
    location: Optional[str] = None
    metadata: Optional[str] = None
    last_seen: Optional[datetime] = None
    created_at: datetime
    updated_at: datetime

    model_config = {"from_attributes": True}


class DeviceHeartbeat(BaseModel):
    uid: str
