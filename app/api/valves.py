from fastapi import APIRouter, HTTPException, status
from app.crud.valve import valve_crud
from app.crud.device import device_crud
from app.schemas.valve import ValveCommandCreate, ValveCommandResponse
from app.core.deps import SessionDep

router = APIRouter(prefix="/valves", tags=["Valves"])


@router.post("/commands", response_model=ValveCommandResponse, status_code=status.HTTP_201_CREATED)
async def issue_command(data: ValveCommandCreate, db: SessionDep):
    device = await device_crud.get_by_uid(db, data.device_uid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return await valve_crud.create_command(db, data, device.id)


@router.get("/{device_uid}/commands/pending", response_model=list[ValveCommandResponse])
async def get_pending_commands(device_uid: str, db: SessionDep):
    device = await device_crud.get_by_uid(db, device_uid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return await valve_crud.get_pending_by_device(db, device.id)


@router.post("/commands/{command_id}/sent", response_model=ValveCommandResponse)
async def mark_command_sent(command_id: int, db: SessionDep):
    cmd = await valve_crud.mark_sent(db, command_id)
    if not cmd:
        raise HTTPException(status_code=404, detail="Command not found")
    return cmd


@router.post("/commands/{command_id}/acknowledge", response_model=ValveCommandResponse)
async def acknowledge_command(command_id: int, db: SessionDep):
    cmd = await valve_crud.mark_acknowledged(db, command_id)
    if not cmd:
        raise HTTPException(status_code=404, detail="Command not found")
    return cmd


@router.get("/{device_uid}/commands/history", response_model=list[ValveCommandResponse])
async def get_command_history(device_uid: str, db: SessionDep, limit: int = 50):
    device = await device_crud.get_by_uid(db, device_uid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return await valve_crud.get_history(db, device.id, limit)
