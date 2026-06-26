from typing import Annotated
from fastapi import Depends, HTTPException, status
from fastapi.security import APIKeyHeader
from sqlalchemy.ext.asyncio import AsyncSession
from app.db import get_db
from app.crud.device import device_crud

api_key_header = APIKeyHeader(name="X-API-Key", auto_error=False)


async def verify_api_key(
    api_key: Annotated[str | None, Depends(api_key_header)],
    db: Annotated[AsyncSession, Depends(get_db)],
):
    if not api_key:
        return None
    device = await device_crud.get_by_uid(db, api_key)
    if not device:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid API key",
        )
    return device


SessionDep = Annotated[AsyncSession, Depends(get_db)]
DeviceDep = Annotated[dict | None, Depends(verify_api_key)]
