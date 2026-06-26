# Smart Farm API – Deployment Guide

## Step 1: Push code to GitHub

From your local machine (where we built the code):

```bash
cd smart-farm-api
git init
git add .
git commit -m "Initial scaffold: FastAPI + SQLAlchemy + MariaDB"
git remote add origin https://github.com/kagwanyu/SMART-FARM.git
git push -u origin main
```

## Step 2: Deploy to server

SSH into the server as **root**:

```bash
ssh root@64.227.174.0
```

Then run the deploy script:

```bash
curl -sL https://github.com/kagwanyu/SMART-FARM/raw/main/deploy.sh | bash
```

Or clone and run:

```bash
apt-get install -y git
git clone https://github.com/kagwanyu/SMART-FARM.git /home/naila/SMART-FARM
cd /home/naila/SMART-FARM
bash deploy.sh
```

## Step 3: Verify

| Endpoint | URL |
|---|---|
| API root | http://64.227.174.0:8000 |
| Swagger docs | http://64.227.174.0:8000/docs |
| Health check | http://64.227.174.0:8000/health |

## Step 4: Show supervisor

Send them two things:
1. **GitHub**: https://github.com/kagwanyu/SMART-FARM
2. **Live docs**: http://64.227.174.0:8000/docs
