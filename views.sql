CREATE TEMPORARY VIEW job_table_view
AS
    SELECT active_jobs.id AS ID,
           active_jobs.job_id AS Label,
           (SELECT name FROM job_states WHERE id = active_jobs.job_state_id) AS State,
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
    FROM volatile.active_jobs
    INNER JOIN main.jobs ON jobs.id = active_jobs.id
    LEFT JOIN volatile.processes ON processes.job_id = active_jobs.id
    ORDER BY Label;