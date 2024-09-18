BEGIN           { inhibit=1 }
END             { force=1 }
force==1 || $1 != LAST_STAR { if(inhibit != 0) {
                   inhibit = 0;
		     } else {
		       if(num > 1) {
			 avg = sum/num;
			 sigma = sqrt((sum_sq - num*avg*avg)/(num - 1));
			 if(sigma > 0.2) {
			 print LAST_STAR, "sigma = ", sigma, " with num = ", num
			   }
		       }
		       num = 0;
		       sum = 0;
		       sum_sq = 0;
		     }
                   LAST_STAR = $1
		     }
                   { loc = match($0, "MV=");
		   if (loc == 0) { next; }
		   split(substr($0, loc+3), words);
		   value = words[1];
		   sum += value;
		   sum_sq += (value*value);
		   num++;
		   }

		   
