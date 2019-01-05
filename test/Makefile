all:
	/bin/false

debug-db:
	sqlite3 --header \
	    --cmd 'attach database "obj/run/jobd/volatile.db" as volatile' \
	    --cmd '.read ../views.sql' \
	    --cmd '.mode column' \
	    --cmd 'select * from temp.job_table_view;' \
	    obj/repository.db # '$(SQL)'
