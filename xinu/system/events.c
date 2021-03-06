/* events.c - */

#include <xinu.h>

/*--------------------------------Local data---------------------------*/
#define INVALID_HANDLER (NULL)
#define INVALID_PID     (NPROC + 1)
#define WILDCARD_GP     (0)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE 
#define FALSE 0
#endif

typedef struct _subscriber_tuple {
	pid32 pid;
	topic_handler handler;
}subscriber_tuple_t;

typedef struct  _subscriber_info {
	uint8 gpid;
	uint8 no_gpsubscbr;
	subscriber_tuple_t pid2handler_map[MAX_NO_SUBSCRIBER];
	struct _subscriber_info *next;
}subscriber_info_t;

typedef struct _topic_data {
	uint8 gpid;
	void *data;
	uint32 size;
	struct _topic_data *next;
}topic_data_t;

typedef struct _topic_data_tuple {
	topic16 topic;
	void *data;
	uint32 size;
}topic_data_tuple_t;

typedef struct _topic_info {
	uint8 no_subscbr;
	subscriber_info_t *subinfo_head;
	topic_data_t *topicd_head;
}topic_t;

topic_t topics[MAX_NO_TOPICS];
static process broker();
pid32 bkr;
/*----------------------------------------------------------------------*/

/*----------------------------------Private Funcions--------------------*/

/* utility function to verify topic-sanity */
static bool8 is_valid_topic(topic16 topic) {
	uint8 _topic = topic & 0xff;
	if (_topic >= MAX_NO_TOPICS) {
		kprintf("Invalid topic - expected to be 0-255\n");
		return FALSE;
	}

	return TRUE;
}

/* utility function to retrun relevant pointer to subscriber-info if such a
 * node exist which matches the gpid */
static subscriber_info_t *get_subcbr_if_gp_exist(uint8 gpid, topic_t *_topic) {
	subscriber_info_t *sub_info_head = _topic->subinfo_head;
	subscriber_info_t *temp          = NULL;

	if (sub_info_head != NULL) {
		temp = sub_info_head;
		while (temp != NULL) {
			if (temp->gpid == gpid) {
				break;
			}
			temp = temp->next;
		}
	}

	return temp;
}

/* utility function to allocate and intitialize subscriber-info-node */
static subscriber_info_t *alloc_n_init_subscbr_info_node() {
	subscriber_info_t *node = NULL;
	uint8 count;

	node = (subscriber_info_t *) getmem(sizeof(subscriber_info_t));

	if ((char *)node != (char *)SYSERR) {
		node->gpid         = 0;
		node->no_gpsubscbr = 0;
		node->next         = NULL;
		
		for (count = 0; count < MAX_NO_SUBSCRIBER; count++) {
			node->pid2handler_map[count].pid     = INVALID_PID;
			node->pid2handler_map[count].handler = INVALID_HANDLER;
		}
	}
	else {
		panic("mem-allocation failed\n");
	}

	return node;
}

/* utility function to aid system-call subscribe */
static status add_subscriber(topic_t *_topic, topic16 topic, pid32 pid, topic_handler handler)
{
	uint8 count;
	uint8 gpid;
	subscriber_info_t *subcbr_node = NULL;
	status result = OK;

	if (_topic->no_subscbr >= MAX_NO_SUBSCRIBER) {
		kprintf("More than %u subscribers not allowed\n", MAX_NO_SUBSCRIBER);
		return SYSERR;
	}

	/* derive group-id and verify if subscription is allowed */
	gpid = (topic & 0xff00) >> 8;

	subcbr_node = get_subcbr_if_gp_exist(gpid, _topic);

	if (subcbr_node) {
		/* add new subscriber to existing group-entry */
		if (subcbr_node->no_gpsubscbr > _topic->no_subscbr) {
			kprintf("gpid:%u topicid:%x %u %u\n", gpid, topic, subcbr_node->no_gpsubscbr, _topic->no_subscbr);
			panic("group-entry can't have more subscribers than the topic itself\n");
		}

		/* check if process already subscribed to the group
		 * in which case only the handler will be replaced */
		for (count = 0; count < MAX_NO_SUBSCRIBER; count++) {
			if (subcbr_node->pid2handler_map[count].pid == pid) {
				break;
			}
		}

		if (count != MAX_NO_SUBSCRIBER) {
			subcbr_node->pid2handler_map[count].handler = handler;
			/* no need to increment count */
		}
		else {
			/* pid do not already exist. look for an empty slot. */
			for (count = 0; count < MAX_NO_SUBSCRIBER; count++) {
				if (subcbr_node->pid2handler_map[count].pid == INVALID_PID 
					&& subcbr_node->pid2handler_map[count].handler == INVALID_HANDLER) {
					break;
				}
			}

			if (count == MAX_NO_SUBSCRIBER) {
				panic("search did not find empty slot to add subscriber but no_subscbr is not max yet\n");
			}
			subcbr_node->pid2handler_map[count].pid     = pid;
			subcbr_node->pid2handler_map[count].handler = handler;
			/* handler added - increment subscriber_count */
			subcbr_node->no_gpsubscbr++;
			_topic->no_subscbr++;
		}
	}
	else {
		/* group does not exist already. add new subscriber
		 * to new group-entry */
		subcbr_node = alloc_n_init_subscbr_info_node();
		subcbr_node->gpid = gpid;
		subcbr_node->no_gpsubscbr++;
		subcbr_node->next = NULL;
		subcbr_node->pid2handler_map[0].pid     = pid;
		subcbr_node->pid2handler_map[0].handler = handler;
		/* add node to the head of subscriber list */
		subcbr_node->next    = _topic->subinfo_head;
		_topic->subinfo_head = subcbr_node;
		_topic->no_subscbr++;
	}

	return result;
}

/* utility function delete subscriber from the topic table */
static status delete_subscriber(topic_t *_topic, topic16 topic, pid32 pid)
{
	status result = OK;
	subscriber_info_t *node = NULL, *temp = NULL;
	uint8 count;
	uint8 gpid;

	gpid = (topic & 0xff00) >> 8;

	/* check validity of the unsubscription request */
	if(_topic->no_subscbr == 0) {
		kprintf("no subscriber exists\n");
		return SYSERR;
	}

	node = get_subcbr_if_gp_exist(gpid, _topic);

	if (node == NULL) {
		kprintf("gp do not exist for topic\n");
		kprintf("subscriber may not be registered\n");
		return SYSERR;
	}

	/* look for the entry */
	for (count = 0; count < MAX_NO_SUBSCRIBER; count++) {
		if (node->pid2handler_map[count].pid == pid) {
			break;
		}
	}

	if (count == MAX_NO_SUBSCRIBER) {
		/* entry not found */
		kprintf("subscriber do not exist for topic 0x%x\n", topic);
		result  = SYSERR;
	}
	else {
		node->no_gpsubscbr--;
		if (node->no_gpsubscbr == 0) {
			/* update linked list and delete node */
			if (_topic->subinfo_head == node) {
				_topic->subinfo_head = NULL;
			}
			else {
				temp = _topic->subinfo_head;
				while (temp != NULL && temp->next != node) {
					temp = temp->next;
				}
				if (temp == NULL) {
					panic("could not find node in subscriber-list\n");
				}
				temp->next = node->next;
			}
			freemem((char *)node, sizeof(subscriber_info_t));
		}
		else {
			/* entry found. invalidate subscription info. */
			node->pid2handler_map[count].pid     = INVALID_PID;
			node->pid2handler_map[count].handler = INVALID_HANDLER;
		}
		_topic->no_subscbr--;
	}

	return result;	
}

/* utility function to add topic to topic table as part of publish */
static status add_to_topic(topic16 topic, topic_t *_topic, void *data, uint32 size)
{
	topic_data_t *new_data  = NULL;
	topic_data_t **topich   = &_topic->topicd_head;
	topic_data_t *temp      = NULL;
	subscriber_info_t *node = NULL;
	uint8 gpid;

	gpid = (topic & 0xff00) >> 8;
	if(_topic->no_subscbr != 0) {
		if(gpid != WILDCARD_GP) {
			/* if the group-id is not wildcard group
			 * and then do not publish if the gp-id
			 * matching subscriber is not present */
			node = _topic->subinfo_head;
			while (node != NULL) {
				if (node->gpid == gpid) {
					break;
				}
				node = node->next;
			}
			if (node == NULL) {
				kprintf("no subscriber to this publish\n");	
				return OK;
			}
		}
	}
	else {
		/* if there is no subscriber then we are ignoring publish */
		kprintf("no subscriber\n", data);	
		return OK;
	}

	/* mem-alloc attempt */
	new_data = (topic_data_t *) getmem(sizeof(topic_data_t));

	/* mem-alloc failed */
	if ((char *)new_data == (char *)SYSERR) {
		panic("mem-failed->publish-failed\n");
		return SYSERR;
	}

	new_data->gpid = gpid;
	new_data->data = (void*) getmem(size);
	if ((char *)new_data->data == (char *)SYSERR) {
		panic("mem-failed->publish-failed\n");
	}
	memcpy(new_data->data, data, size);
	new_data->size = size;
	new_data->next = NULL;

	/* add topic to queue */
	if (*topich == NULL) {
		*topich = new_data;
	}
	else {
		temp = *topich;
		while (temp->next) {
			temp = temp->next;
		}

		temp->next = new_data;
	}

	return OK;
}

/* utility function to get topic data from topic-queue */
static void *get_topic_data(topic_data_t **topich, topic16 *topic, uint32 *size) {
	topic_data_t *temp;
	void *data;

	/* get data from queue head */
	temp    = *topich;
	*topich = (*topich)->next;

	/* store data to return */
	data = temp->data;
	*size = temp->size;
	*topic = (temp->gpid << 8) & 0xff00;
	/* free slot */
        freemem((char *)temp, sizeof(topic_data_t));

	return data;
}

/* utility function to get next pending publish from the topic table */
static topic_data_tuple_t *get_next_pending_publish()
{
	uint16 count;
	topic_data_tuple_t *topic_data = NULL;

	/* look for pending publish in topic-table */
	for (count = 0; count < MAX_NO_TOPICS; count++) {
		if (topics[count].topicd_head != NULL) {
			topic_data = (topic_data_tuple_t *) getmem(sizeof(topic_data_tuple_t));
			if ((char *)topic_data != (char *)SYSERR) {
				topic_data->data  = get_topic_data(&topics[count].topicd_head, &topic_data->topic, &topic_data->size);
				topic_data->topic |= (count & 0x00ff);
			}
			break;
		}
	}

	return topic_data;
}

/*--------------------------------Public Functions and system-call definitions---------------------*/
/* initialize topic-table */
void init_topics () {
	uint16 count;
	topic_t *_topic;

	for (count = 0; count < MAX_NO_TOPICS; count++) {
		_topic = &topics[count];
		_topic->no_subscbr   = 0;
		_topic->subinfo_head = NULL;
		_topic->topicd_head  = NULL;
	}
	bkr = create(broker, 4096, 20, "broker_process", 0, NULL);
	resume(bkr);
}

/* system call to subscribe for topic */
syscall subscribe(topic16 topic, topic_handler handler)
{
	topic_t *_topic;
	pid32 pid = getpid();
	intmask mask;
	status result = SYSERR;

	mask = disable();
	/* kprintf("subsc>>\n"); */
	if (!is_valid_topic(topic)) {
		restore(mask);
		return result;
	}

	_topic = &topics[topic & 0xff];

	if (isbadpid(pid) || (handler == NULL)) {
		restore(mask);
		return result;
	}

	result = add_subscriber(_topic, topic, pid, handler);

	/* kprintf("subsc<<\n"); */
	restore(mask);
	return result;	
}

/* system call to ubsubscribe from a topic */
syscall unsubscribe(topic16 topic)
{
	topic_t *_topic;
	pid32 pid = getpid();
	intmask mask;
	status result = SYSERR;

	mask = disable();
	/* kprintf("un-subsc>>\n"); */
	if (!is_valid_topic(topic)) {
		restore(mask);
		return result;
	}
	
	_topic = &topics[topic & 0xff];

	if (isbadpid(pid)) {
		restore(mask);
		return result;
	}
	result = delete_subscriber(_topic, topic, pid);

	/* kprintf("un-subsc<<\n"); */
	restore(mask);
	return result;	
}

/* system call to publish data to a topic */
syscall publish(topic16 topic, void *data, uint32 size)
{
	topic_t *_topic;
	intmask mask;
	status result = SYSERR;

	mask = disable();
	/* kprintf("publish>>\n"); */
	if (!is_valid_topic(topic)) {
		restore(mask);
		return result;
	}

	_topic = &topics[topic & 0xff];

	result = add_to_topic(topic, _topic, data, size);

	/* kprintf("publish<<\n"); */
	restore(mask);
	return result;
}

/* de-initialize topic-table */
void deinit_topics () {
	uint16 count;
	topic_t           *_topic = NULL;
	subscriber_info_t *node   = NULL, *pnode = NULL;
	topic_data_t      *tnode  = NULL, *ptnode = NULL;

	for (count = 0; count < MAX_NO_TOPICS; count++) {
		_topic = &topics[count];
		if (_topic->subinfo_head != NULL) {
			node = _topic->subinfo_head;
			while(node != NULL) {
				pnode = node;
				node  = node->next;
				freemem((char *)pnode, sizeof(subscriber_info_t));
			}
		}
		if (_topic->topicd_head != NULL) {
			tnode = _topic->topicd_head;
			while(tnode != NULL) {
				ptnode = tnode;
				tnode  = tnode->next;
				freemem((char *)ptnode, sizeof(topic_data_t));
			}
		}
	}
	kill(bkr);
}

/*-------------------------Broker process and relevant untility function-----------------------*/

static void send_data_to_respective_subscriber(topic_t *_topic, topic_data_tuple_t *topic_data) {
	subscriber_info_t *node = _topic->subinfo_head;
	topic16 topicid;
	uint8 gpid, count;
	
	/* if topic_data pertains to wildcard gropu then traverse throug
	 * the entire subscriber list and publish to each of the publisher,
	 * otherwise publish only to the concerned group publishers */
	if (node == NULL) {
		/* no subscribers present. return */
		return;
	}

	gpid = (topic_data->topic >> 8) & 0xff;
	if (!gpid) {
		/* if wildcard publish */
		topicid = topic_data->topic & 0x00ff;

		while (node != NULL) {
			for (count = 0; count < MAX_NO_SUBSCRIBER; count++) {
				if (node->pid2handler_map[count].pid != INVALID_PID 
					&& node->pid2handler_map[count].handler != INVALID_HANDLER) {
					node->pid2handler_map[count].handler(topicid, topic_data->data, topic_data->size);
				}
			}
			node = node->next;
		}
	}
	else {
		/* if not wildcard */
		topicid = topic_data->topic;

		while (node != NULL) {
			if (node->gpid == gpid) {
				break;
			}
			node = node->next;
		}

		if (node != NULL) {
			for (count = 0; count < MAX_NO_SUBSCRIBER; count++) {
				if (node->pid2handler_map[count].pid != INVALID_PID 
					&& node->pid2handler_map[count].handler != INVALID_HANDLER) {
					node->pid2handler_map[count].handler(topicid, topic_data->data, topic_data->size);
				}
			}
		}
	}
}

/* process to monitor published-data and call respective callbacks */
process broker()
{
	topic_data_tuple_t  *topic_data;
	topic_t *_topic;

	while (TRUE) {
		topic_data = get_next_pending_publish();

		if (topic_data != NULL) {
			_topic = &topics[topic_data->topic & 0x00ff];

			if (_topic->no_subscbr != 0) {
				send_data_to_respective_subscriber(_topic, topic_data);
			}
			if ((char *)topic_data->data != (char *)SYSERR) {
				freemem((char *)topic_data->data, topic_data->size);
			}
			freemem((char *)topic_data, sizeof(topic_data_tuple_t));
		}	
	}
	return OK;
} 
