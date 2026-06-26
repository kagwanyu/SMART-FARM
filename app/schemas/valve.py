from datetime import datetime
from pydantic import BaseModel
from typing import Optional
from app.models.valve import CommandType, CommandStatus


class ValveCommandCreate(BaseModel):
    device_uid: str
    command: CommandType
    position: Optional[int] = None
    issued_by: Optional[str] = None


class ValveCommandResponse(BaseModel):
    id: int
    device_id: int
    zone_id: Optional[int] = None
    command: CommandType
    position: Optional[int] = None
    status: CommandStatus
    issued_by: Optional[str] = None
    sent_at: Optional[datetime] = None
    acknowledged_at: Optional[datetime] = None
    created_at: datetime

    model_config = {"from_attributes": True}
