-- A volatile database living in $runstatedir that is recreated
-- at every boot.

PRAGMA volatile.synchronous = OFF;

CREATE TABLE volatile.processes (
    pid INTEGER PRIMARY KEY,    -- matches kernel PID
    job_id INTEGER NOT NULL,
    process_state_id INTEGER NOT NULL DEFAULT 1,
    FOREIGN KEY (process_state_id) REFERENCES volatile.process_states (id) ON DELETE RESTRICT,
    FOREIGN KEY (job_id) REFERENCES main.jobs (id) ON DELETE RESTRICT
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

CREATE VIEW process_table_view
AS
SELECT 
  jobs.id,
  jobs.job_id,
  processes.pid AS pid,
  processes.process_state_id AS state_id,
  process_states.name AS state
FROM jobs 
INNER JOIN processes ON jobs.id = processes.job_id 
INNER JOIN process_states on processes.process_state_id = process_states.id
ORDER BY jobs.job_id;
