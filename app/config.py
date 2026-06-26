from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    API_NAME: str = "Smart Farm API"
    API_VERSION: str = "1.0.0"
    DEBUG: bool = False

    DB_HOST: str = "localhost"
    DB_PORT: int = 3306
    DB_USER: str = "smartfarm"
    DB_PASSWORD: str = ""
    DB_NAME: str = "smartfarm"

    @property
    def DATABASE_URL(self) -> str:
        return (
            f"mysql+aiomysql://{self.DB_USER}:{self.DB_PASSWORD}"
            f"@{self.DB_HOST}:{self.DB_PORT}/{self.DB_NAME}"
        )

    SECRET_KEY: str = "change-me-in-production"
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 1440

    class Config:
        env_file = ".env"


settings = Settings()
