-- A volatile database living in $runstatedir that is recreated
-- at every boot.

BEGIN TRANSACTION;

CREATE TABLE volatile.processes (
    pid INTEGER PRIMARY KEY,    -- matches kernel PID
    job_id INTEGER NOT NULL,
    process_state_id INTEGER NOT NULL DEFAULT 1,
    FOREIGN KEY (process_state_id) REFERENCES process_states (id) ON DELETE RESTRICT
    -- FIXME: will not work due to sqlite limitation: FOREIGN KEY (job_id) REFERENCES main.jobs (id) ON DELETE RESTRICT
);

CREATE TABLE volatile.process_states (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL
);
INSERT INTO process_states (id, name) VALUES (1, 'uninitialized');
INSERT INTO process_states (id, name) VALUES (2, 'starting');
INSERT INTO process_states (id, name) VALUES (3, 'running');
INSERT INTO process_states (id, name) VALUES (4, 'stopping');
INSERT INTO process_states (id, name) VALUES (5, 'stopped');
INSERT INTO process_states (id, name) VALUES (6, 'error');

-- CREATE VIEW IF NOT EXISTS process_table_view
-- AS
-- SELECT 
--   jobs.id,
--   jobs.job_id,
--   processes.pid AS pid,
--   processes.process_state_id AS state_id,
--   process_states.name AS state
-- FROM jobs 
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

COMMIT;