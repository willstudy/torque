/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/


#include <pthread.h>
#include "container.hpp"
#include <vector>
#include <string>
#include <set>
#include <map>
#include "pbs_job.h"
#include "utils.h"

#define INITIAL_NODE_LIST_SIZE          10
#define INITIAL_RESERVATION_HOLDER_SIZE 100


class alps_reservation
  {
public:
  std::string              rsv_id;         /* the reservation id */
  std::vector<std::string> ar_node_names;/* the node ids associated with this reservation */
  int                      internal_job_id; /* the job id associated with this reservation */

  alps_reservation() : rsv_id(), internal_job_id(-1)
    {
    }

  alps_reservation(const alps_reservation &other) : rsv_id(other.rsv_id),
                                                    ar_node_names(other.ar_node_names),
                                                    internal_job_id(other.internal_job_id)
    {
    }

  alps_reservation(int job_int_id, const char *new_rsvid) : internal_job_id(job_int_id)
    {
    this->rsv_id = new_rsvid;
    }

  alps_reservation(job *pjob) : rsv_id(), ar_node_names(), internal_job_id(pjob->ji_internal_id)
    {
    if (pjob->ji_wattr[JOB_ATR_reservation_id].at_val.at_str != NULL)
      {
      this->rsv_id = pjob->ji_wattr[JOB_ATR_reservation_id].at_val.at_str;
      }
    
    char *exec_str = strdup(pjob->ji_wattr[JOB_ATR_exec_host].at_val.at_str);
    char *host_tok;
    char *str_ptr = exec_str;
    char *slash;
    int   rc = PBSE_NONE;
    char *prev_node = NULL;

    while ((host_tok = threadsafe_tokenizer(&str_ptr, "+")) != NULL)
      {
      if ((slash = strchr(host_tok, '/')) != NULL)
        *slash = '\0';

      if ((prev_node == NULL) ||
          (strcmp(prev_node, host_tok)))
        {
        this->ar_node_names.push_back(host_tok);
        rc = PBSE_NONE;
        }

      prev_node = host_tok;
      }

    if (exec_str != NULL)
      free(exec_str);
    }

  alps_reservation &operator =(const alps_reservation &other)
    {
    this->rsv_id = other.rsv_id;

    this->ar_node_names = other.ar_node_names;
    this->internal_job_id = other.internal_job_id;

    return(*this);
    }

  ~alps_reservation()
    {
    }
  };


class reservation_holder
  {
  std::map<std::string, alps_reservation> reservations;
  std::set<std::string>                   orphaned_reservations;
  pthread_mutex_t                         rh_mutex;

  public:
  reservation_holder();

  int  track_alps_reservation(job *pjob);
  int  remove_alps_reservation(const char *rsv_id);
  bool is_orphaned(const char *rsv_id, std::string &job_id);
  bool already_recorded(const char *rsv_id);
  int  insert_alps_reservation(const alps_reservation &ar);
  void remove_from_orphaned_list(const char *rsv_id);
  void clear();
  int  count();
  };

extern reservation_holder alps_reservations;


