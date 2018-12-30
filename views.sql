CREATE TEMPORARY VIEW job_table_view
AS
    SELECT active_jobs.id AS ID,
           active_jobs.job_id AS Label,
           (SELECT name FROM job_states WHERE id = active_jobs.job_state_id) AS Status,
           (SELECT name FROM job_types WHERE id = job_type_id) AS "Type"
    FROM volatile.active_jobs
    LEFT JOIN main.jobs ON jobs.id = active_jobs.id
    ORDER BY Label;