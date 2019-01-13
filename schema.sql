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
CREATE TABLE job_depends
(
    id            INTEGER PRIMARY KEY,
    before_job_id TEXT NOT NULL
        REFERENCES jobs (job_id)
            ON DELETE CASCADE
            DEFERRABLE INITIALLY DEFERRED,
    after_job_id  TEXT NOT NULL
        REFERENCES jobs (job_id)
            ON DELETE CASCADE
            DEFERRABLE INITIALLY DEFERRED
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

CREATE TABLE job_states
(
    id   INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL
);

-- Keep this in sync with enum job_state.
INSERT INTO job_states (id, name)
VALUES
    (1, 'disabled'),
    (2, 'pending'),
    (3, 'starting'),
    (4, 'running'),
    (5, 'stopping'),
    (6, 'stopped'),
    (7, 'complete'),
    (8, 'error');

CREATE TABLE processes
(
    pid           INTEGER PRIMARY KEY, -- matches kernel PID
    job_id        INTEGER UNIQUE NOT NULL,
    exited        INTEGER CHECK (exited IN (0, 1)),
    exit_status   INTEGER,
    signaled      INTEGER CHECK (signaled IN (0, 1)),
    signal_number INTEGER,
    start_time    INTEGER        NOT NULL DEFAULT 0,
    end_time      INTEGER        NOT NULL DEFAULT 0,
    FOREIGN KEY (job_id) REFERENCES jobs (id) ON DELETE RESTRICT
);

CREATE TABLE jobs_current_states
(
    id           INTEGER PRIMARY KEY,
    job_id       INTEGER UNIQUE NOT NULL,
    job_state_id INTEGER NOT NULL,
    FOREIGN KEY (job_id) REFERENCES jobs (id) ON DELETE CASCADE,
    FOREIGN KEY (job_state_id) REFERENCES job_states (id) ON DELETE RESTRICT
);

CREATE VIEW jobs_current_states_view AS
SELECT jobs.job_id, job_states.name
FROM jobs_current_states
         LEFT JOIN jobs ON jobs.id = jobs_current_states.job_id
         LEFT JOIN job_states ON job_states.id = jobs_current_states.job_state_id
ORDER BY jobs.job_id;


CREATE VIEW runnable_jobs AS
SELECT jobs.id FROM jobs
JOIN jobs_current_states ON jobs.id = jobs_current_states.job_id
WHERE jobs.id NOT IN (SELECT job_id AS id FROM processes)
AND job_state_id = (SELECT id FROM job_states WHERE name = 'pending');

CREATE VIEW job_table_view
AS
SELECT jobs.id AS ID,
       jobs.job_id AS Label,
       (SELECT name FROM job_states WHERE id = jobs.job_state_id) AS State,
       (SELECT name FROM job_types WHERE id = job_type_id) AS "Type",
       CASE
           WHEN processes.exited = 1 THEN 'exit(' || processes.exit_status || ')'
           WHEN processes.signaled = 1 THEN 'kill(' || processes.signal_number || ')'
           ELSE '-'
           END Terminated,
       CASE
           WHEN processes.end_time = 0 THEN (strftime('%s','now') - processes.start_time) || 's'
           ELSE (processes.end_time - processes.start_time) || 's'
           END Duration
FROM jobs
LEFT JOIN processes ON processes.job_id = jobs.id
ORDER BY Label;