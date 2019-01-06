-- NOTE: requires a compile-time setting to allow this
PRAGMA foreign_keys = ON;

CREATE TABLE job_types
(
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL
);

INSERT INTO job_types (id, name)
VALUES
    (1, 'task'),
    (2, 'service');

CREATE TABLE jobs (
    id INTEGER PRIMARY KEY,
    job_id TEXT UNIQUE NOT NULL,
    job_type_id INTEGER NOT NULL,
    command TEXT NOT NULL, -- DEADWOOD: should rip this out in favor of stop/start methods
    description VARCHAR,
    wait BOOLEAN NOT NULL DEFAULT 0 CHECK (wait IN (0,1)),
    gid VARCHAR,
    init_groups BOOLEAN NOT NULL DEFAULT 1 CHECK (init_groups IN (0,1)),
    keep_alive BOOLEAN NOT NULL DEFAULT 0 CHECK (keep_alive IN (0,1)),
    root_directory VARCHAR NOT NULL DEFAULT '/',
    standard_error_path VARCHAR NOT NULL DEFAULT '/dev/null',
    standard_in_path NOT NULL DEFAULT '/dev/null',
    standard_out_path NOT NULL DEFAULT '/dev/null',
    start_order INT,
    umask VARCHAR,
    user_name VARCHAR,
    working_directory VARCHAR NOT NULL DEFAULT '/',
    FOREIGN KEY (job_type_id) REFERENCES job_types (id) ON DELETE RESTRICT
);

CREATE TABLE job_methods (
    id INTEGER PRIMARY KEY,
    job_id INTEGER NOT NULL,
    name TEXT NOT NULL,  -- start, stop, etc.
    script TEXT NOT NULL, -- a sh(1) script
    FOREIGN KEY (job_id) REFERENCES jobs (id) ON DELETE CASCADE
);

-- Ordering: the "before_job_id" will be started before the "after_job_id"
CREATE TABLE job_depends (
    id INTEGER PRIMARY KEY,
    before_job_id TEXT NOT NULL,
    after_job_id TEXT NOT NULL,
    FOREIGN KEY (before_job_id) REFERENCES jobs (job_id) ON DELETE CASCADE,
    FOREIGN KEY (after_job_id) REFERENCES jobs (job_id) ON DELETE CASCADE
);

-- Environment variables for each job.
CREATE TABLE jobs_environment (
    id INTEGER PRIMARY KEY,
    job_id INTEGER,
    env_key TEXT NOT NULL,
    env_value TEXT NOT NULL,
    UNIQUE (job_id, env_key),
    FOREIGN KEY (job_id) REFERENCES jobs (id) ON DELETE CASCADE 
);


---
--- JOB PROPERTIES
---

--- names for datatypes. Keep this in sync with parser.h datatypes.
CREATE TABLE datatypes
(
    id   INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL
);
INSERT INTO datatypes (id, name)
VALUES
    (1, 'integer'),
    (2, 'string'),
    (3, 'boolean');

CREATE TABLE properties
(
    id            INTEGER PRIMARY KEY,
    job_id        INTEGER NOT NULL,
    datatype_id   INTEGER NOT NULL,
    name          TEXT    NOT NULL,
    default_value TEXT    NOT NULL,
    current_value TEXT    NOT NULL,
    UNIQUE (job_id, name),
    FOREIGN KEY (datatype_id) REFERENCES datatypes (id) ON DELETE RESTRICT,
    FOREIGN KEY (job_id) REFERENCES jobs (id) ON DELETE CASCADE
);

CREATE VIEW properties_view
AS
SELECT id,
       job_id,
       (SELECT jobs.job_id FROM jobs WHERE jobs.id = properties.job_id) AS job_name,
       (SELECT name FROM datatypes WHERE datatypes.id = datatype_id)    AS datatype,
       name,
       current_value AS value,
       (name || '=''' || replace(current_value, '''', '''"''"''') || '''') AS shellcode,
    CASE
    WHEN current_value = default_value THEN 'default'
    ELSE 'custom'
END
source
FROM
properties;
