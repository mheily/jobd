-- A volatile database living in $runstatedir that is recreated
-- at every boot.

BEGIN TRANSACTION;

CREATE TABLE volatile.processes (
    pid INTEGER PRIMARY KEY,    -- matches kernel PID
    job_id INTEGER UNIQUE NOT NULL,
    exited INTEGER CHECK (exited IN (0,1)),
    exit_status INTEGER,
    signaled INTEGER CHECK (signaled IN (0,1)),
    signal_number INTEGER,
    process_state_id INTEGER NOT NULL DEFAULT 1,
    FOREIGN KEY (process_state_id) REFERENCES process_states (id) ON DELETE RESTRICT
    -- FIXME: will not work due to sqlite limitation: FOREIGN KEY (job_id) REFERENCES main.jobs (id) ON DELETE RESTRICT
);

CREATE TABLE volatile.process_states (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL
);
INSERT INTO process_states (id, name) VALUES (1, 'starting');
INSERT INTO process_states (id, name) VALUES (2, 'running');
INSERT INTO process_states (id, name) VALUES (3, 'stopping');
INSERT INTO process_states (id, name) VALUES (4, 'stopped');
INSERT INTO process_states (id, name) VALUES (5, 'error');

-- CREATE VIEW IF NOT EXISTS volatile.process_table_view
-- AS
-- SELECT 
--   main.jobs.id,
--   main.jobs.job_id,
--   processes.pid AS pid,
--   processes.process_state_id AS state_id,
--   process_states.name AS state,
--   processes.exit_status,
--   processes.signal_number
-- FROM main.jobs 
-- INNER JOIN processes ON jobs.id = processes.job_id 
-- INNER JOIN process_states on processes.process_state_id = process_states.id
-- ORDER BY jobs.job_id;
 
--- configuration changes requested via jobadm(1)

CREATE TABLE volatile.pending_changes (
    id INTEGER PRIMARY KEY,
    job_id INTEGER NOT NULL,
    change_type_id INTEGER NOT NULL,
    -- FIXME: will not work due to sqlite limitation: FOREIGN KEY (job_id) REFERENCES jobs (id) ON DELETE CASCADE,
    FOREIGN KEY (change_type_id) REFERENCES pending_change_types (id) ON DELETE RESTRICT
);

CREATE TABLE volatile.pending_change_types (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL
);
INSERT INTO pending_change_types VALUES(1, "enable");
INSERT INTO pending_change_types VALUES(2, "disable");

-- the order that jobs are started in
CREATE TABLE volatile.job_order (
    job_id INTEGER UNIQUE NOT NULL, -- FOREIGN KEY of jobs table
    wave INTEGER
);

COMMIT;

--CREATE TABLE volatile.jobs AS SELECT * FROM main.jobs;
