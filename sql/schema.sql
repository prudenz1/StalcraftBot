CREATE TABLE IF NOT EXISTS items (
    id         VARCHAR(16) PRIMARY KEY,
    category   VARCHAR(128) NOT NULL,
    name_ru    VARCHAR(256) NOT NULL,
    tracked    BOOLEAN DEFAULT FALSE
);

CREATE TABLE IF NOT EXISTS lot_snapshots (
    id            BIGSERIAL PRIMARY KEY,
    item_id       VARCHAR(16) REFERENCES items(id),
    buyout_price  BIGINT,
    start_price   BIGINT,
    start_time    TIMESTAMPTZ,
    snapshot_time TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_lot_snapshots_item_time
    ON lot_snapshots(item_id, snapshot_time);

CREATE TABLE IF NOT EXISTS price_snapshots (
    id             BIGSERIAL PRIMARY KEY,
    item_id        VARCHAR(16) REFERENCES items(id),
    timestamp      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    min_price      BIGINT,
    avg_price      BIGINT,
    median_price   BIGINT,
    max_price      BIGINT,
    std_dev        DOUBLE PRECISION,
    lot_count      INT,
    filtered_count INT
);

CREATE INDEX IF NOT EXISTS idx_price_snapshots_item_time
    ON price_snapshots(item_id, timestamp);

CREATE TABLE IF NOT EXISTS hourly_stats (
    id           BIGSERIAL PRIMARY KEY,
    item_id      VARCHAR(16) REFERENCES items(id),
    hour         SMALLINT NOT NULL CHECK (hour BETWEEN 0 AND 23),
    avg_price    BIGINT,
    sample_count INT,
    updated_at   TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(item_id, hour)
);

CREATE TABLE IF NOT EXISTS alerts (
    id         BIGSERIAL PRIMARY KEY,
    item_id    VARCHAR(16) REFERENCES items(id),
    alert_type VARCHAR(16) NOT NULL,
    rating     DOUBLE PRECISION,
    message    TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_alerts_item_time
    ON alerts(item_id, created_at);
