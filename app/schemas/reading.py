from datetime import datetime
from pydantic import BaseModel, Field
from typing import List, Optional


class ReadingCreate(BaseModel):
    device_uid: str
    metric: str = Field(max_length=64)
    value: float
    unit: Optional[str] = None
    timestamp: Optional[datetime] = None


class ReadingBatchCreate(BaseModel):
    readings: List[ReadingCreate]


class ReadingResponse(BaseModel):
    id: int
    device_id: int
    metric: str
    value: float
    unit: Optional[str] = None
    timestamp: datetime

    model_config = {"from_attributes": True}


class ReadingQuery(BaseModel):
    device_uid: Optional[str] = None
    metric: Optional[str] = None
    from_: Optional[datetime] = Field(None, alias="from")
    to: Optional[datetime] = None
    limit: int = 100
    offset: int = 0
