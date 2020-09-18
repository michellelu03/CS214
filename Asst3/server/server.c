#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <math.h>
#include <dirent.h>
#include<openssl/sha.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>


typedef struct mutex
{
	char* name;
	pthread_mutex_t* lock;
	struct mutex* next;	
}mutex;

//globals
mutex* head = NULL;
int serv_sock;

typedef struct node
{
    char* data;
    struct node* next;
}node;

typedef struct file
{
	int version;
	char* filename;
	char* digest;
	struct file* next;
}file;

typedef struct manifest
{
	int version;
    char* project;
	file** files;
}manifest;

int length;

void* myMalloc(int size)
{
    void* temp = calloc(size,1);
    if(temp == NULL)
    {
        printf("Error: Malloc has returned null, Aborting.\n");
        exit(1);
    }
    return temp;
}

mutex* initMutex(char* projectName) //initializes mutex struct
{
	mutex* temp = myMalloc(sizeof(mutex));
	char* tempName = myMalloc(strlen(projectName));
	memcpy(tempName, projectName, strlen(projectName));
	pthread_mutex_t tempLock;
	if(pthread_mutex_init(&tempLock, NULL) != 0)
	{
		printf("mutex init has failed.\n");
		return NULL;
	}
	temp->lock = &tempLock;
	temp->name = tempName;
	temp->next = NULL;	
	return temp;
}

void freeMutexes()	//frees list of mutexes (at exit)
{
	while(head != NULL)
	{
		mutex* ptr = head;
		head = head->next;
		pthread_mutex_destroy(ptr->lock);
		free(ptr->name);
		free(ptr);
	}
}

void mutex_creat(char* projectName) //creates and adds mutex to LL when project is created
{
	mutex* temp = initMutex(projectName);
	temp->next = head;
	head = temp;
}

void mutex_del(char* projectName)	//deletes mutex
{
	if(head == NULL){
		return;
	}
	mutex* curr = head;
	mutex* prev;
	if(strcmp(head->name, projectName) ==  0){ //delete head
		head = curr->next;
		pthread_mutex_destroy(curr->lock);
		free(head->name);
		free(curr);
		return;
	}
	while(curr != NULL && strcmp(curr->name, projectName) != 0){
		prev = curr;
		curr = curr->next;
	}
	if(curr == NULL){
		return;
	}	
	prev->next = curr->next;
	pthread_mutex_destroy(curr->lock);
	free(curr->name);
	free(curr);
}

mutex* getMutex(char* projectName)	//returns mutex for thread to lock
{
	mutex* ptr = head;
	while(ptr != NULL)
	{
		if(strcmp(projectName, ptr->name) == 0)
			return ptr;
	}
	return NULL;
}

node* initNode()
{
    node* temp = myMalloc(sizeof(node));
    temp->next = NULL;
    temp->data = NULL;
    return temp;
}

file* initFile()
{
	file* temp = myMalloc(sizeof(file));
	temp->version = 0;
	temp->filename = NULL;
	temp->digest = NULL;
	temp->next = NULL;
	return temp;
}

void freeList(node* head)
{
    node* temp;
    while(head!=NULL)
    {
        temp = head;
        head = head->next;
        if(temp->data != NULL && strlen(temp->data)!=0)
            free(temp->data);
        free(temp);
    }
}

node* insert(node* head, node* temp)
{
    node* curr = head;
    node* prev = NULL;
    while(curr != NULL)
    {
        prev = curr;
        curr = curr->next;
    }
    if(prev==NULL)
        head = temp;
    else
        prev->next = temp;
    return head;
}

node* addToList(node* head,node* word)
{
    char* w = myMalloc(length+1);
    int i;
    for(i = 0;i<length;i++)
    {
        w[i] = *word->data;
        word= word->next;
    }
    node* temp = initNode();
    if(length == 0)
        temp->data = "";
    else
        temp->data = w;
    head = insert(head,temp);
    return head;
}

char* getDigest(char* file)
{
	int fd = open(file, O_RDONLY);
	if(fd < 0)
		close(fd);
	SHA_CTX ctx;
	SHA1_Init(&ctx);
	char* c = myMalloc(sizeof(char));
	while(read(fd, c, 1) > 0)
	{
		SHA1_Update(&ctx, c, 1);
	}
	unsigned char tmphash[SHA_DIGEST_LENGTH];
	SHA1_Final(tmphash, &ctx);
	char* hash = myMalloc(SHA_DIGEST_LENGTH*2);
	int i = 0;
	for(i = 0; i < SHA_DIGEST_LENGTH; i++)
	{
  	  sprintf((char*)&(hash[i*2]), "%02x", tmphash[i]);
	}
	free(c);
	close(fd);
	return hash;
}

manifest* initManifest()
{	
	manifest* temp = myMalloc(sizeof(manifest));
	temp->version = 0;
	temp->files = myMalloc(20*sizeof(file*));
	return temp;
}

void createManifestFile(manifest* m,char* path)
{
	remove(path);			
	int wfd = open(path, O_WRONLY | O_APPEND | O_CREAT, 00777);
	if(wfd < 0)
	{
		printf("Error, could not create .Manifest.\n");
		close(wfd);
		return;
	}
	char* buf = myMalloc(32);
	sprintf(buf, "%d\n", m->version);
	write(wfd, buf, strlen(buf));
    free(buf);
    file* temp;
	int i;
	for(i = 0; i < 20; i++)
	{
		temp = m->files[i];
		while(temp != NULL)
		{
            buf = myMalloc(13+strlen(temp->filename)+strlen(temp->digest));
			sprintf(buf, "%d\t%s\t%s\n", temp->version,temp->filename,temp->digest);
			write(wfd, buf, strlen(buf));
            free(buf);
            temp = temp->next;
		}
	}
	close(wfd);
}

manifest* deleteFromManifest(manifest* m, char* filename)
{
	int bucket = (getASCII(filename)) % 20;
	if(m->files[bucket] == NULL)
		return m;
	file* curr = m->files[bucket];
	file* prev;
	if(strcmp(m->files[bucket]->filename, filename) == 0) //delete head
	{ 
		m->files[bucket] = curr->next;
        free(curr->filename);
		free(curr->digest);
		free(curr);
		return m;
	}
	while(curr != NULL && (strcmp(curr->filename, filename) != 0))
	{
		prev = curr;
		curr = curr->next;
	}
	if(curr == NULL)
		return m;
	prev->next = curr->next;
    free(curr->filename);
	free(curr->digest);
	free(curr);
	return m;
}

manifest* updateManifest(manifest* m, char* filename)
{
	int bucket = (getASCII(filename)) % 20;
	file* ptr;
	for(ptr = m->files[bucket]; ptr != NULL; ptr = ptr->next)
    {
		if(strcmp(ptr->filename, filename) == 0)
		{
			ptr->version++;
			ptr->digest = getDigest(filename);
			break;
		}
	}
	return m;
}

manifest* insertToManifest(manifest* m, file* f)
{
	int bucket = (getASCII(f->filename)) % 20;
	f->next = m->files[bucket]; 
	m->files[bucket] = f;
	return m;	
}

node* readFile(char* file)
{
    int fd = open(file,O_RDONLY);
	if(fd <0)
	{
		printf("Error: Could not open file.");
	}
    char* c = myMalloc(sizeof(char));
    node* head = NULL;                  //linked list of chars to create a token
    node* temp;
    node* list = NULL;                  //linked list of tokens from file
    length = 0;
    while(read(fd,c,1) > 0)
    {
        if(*c != '\t' && *c != '\n')
        {
            length+=1;
            temp = initNode();
            temp->data = myMalloc(sizeof(char));
            memcpy(temp->data,c,1);
            head = insert(head,temp);
        }
        else
        {
            list = addToList(list,head);
            length=0;
            freeList(head);
            head = NULL;
        }
    }
    if(length > 0)
        list = addToList(list,head);
    freeList(head);
    if(list == NULL)
    {
        printf("Warning: file is empty\n");
        close(fd);
    }
    free(c);
    return list;
}

manifest* loadManifest(char* manpath)
{
    node* list = readFile(manpath);
    if(list == NULL)
    {
        printf("Error: Something went wrong with reading the .Manifest\n");
        return NULL;
    }   
    manifest* m = initManifest();
    m->version = atoi(list->data);
    node* ptr = list->next;
    file* temp = initFile();
    int i = 1;
    while(ptr!=NULL)
    {
        if(i%3==0)
        {
            temp->digest = myMalloc(strlen(ptr->data)+1);
            memcpy(temp->digest,ptr->data,strlen(ptr->data)+1);
            m = insertToManifest(m,temp);
            temp = initFile();
            i-=2;
        }
        else if(i%2==0)
        {
            temp->filename = myMalloc(strlen(ptr->data)+1);
            memcpy(temp->filename,ptr->data,strlen(ptr->data)+1);
            i++;
        }
        else
        {
            temp->version = atoi(ptr->data);
            i++;
        }
        ptr = ptr->next;
    }
    free(temp);
    freeList(list);
    return m;
}

char* clientMessage(int client_sock)
{
    //get size
    char* c = myMalloc(1);
    char* size = myMalloc(32);
    int i = 0;
    while(read(client_sock,c,1)>0 && *c != '~')
        strcat(size,c);
    //convert size to int
    int siz = atoi(size);
    if(siz <= 0)
    {   
        printf("Something went wrong with delete.\n");
        return strdup("");
    }
    char* fileName = myMalloc(siz);
    i=0;
    for(i;i<siz;i++)
    {
        read(client_sock,c,1);
        strcat(fileName,c);
    }
    free(c);
    free(size);
    return fileName;
}

void freeManifest(manifest* m)
{
	int i;
	for(i = 0; i < 20; i++)
	{
		file* temp = m->files[i];
		while(temp != NULL)
		{
			file* temp2 = temp;
			temp = temp->next;
            if(temp2->filename != NULL)
                free(temp2->filename);
			free(temp2->digest);
			free(temp2);
		}
	}
    free(m->files);
	free(m);
}

int getASCII(char* filename)
{
	int value = 0;
	int i;
	for(i = 0; i < strlen(filename); i++)
		value += (int)filename[i];
	return value;
}

int fileLookup(char* file)
{
    struct dirent* dp;
    DIR* dir = opendir(".");
    dp = readdir(dir);
    dp = readdir(dir);
    dp = readdir(dir);
    while(dp!=NULL)
    {
        if(dp->d_type == 4)
            if(strcmp(dp->d_name,file)==0)
            {
                free(dir);
                return 1;
            }
        dp = readdir(dir);
    }
    free(dir);
    return 0;
}

int mkdirr(char* p,int mode,int fail_on_exist)
{
    int result = 0;
    char* path = myMalloc(strlen(p)+1);
    memcpy(path,p,strlen(p));
    char * dir = NULL;
    do
    {
        if (path == NULL)
        {
            result = -1;
            break;
        }
        //try to chop off last directory
        if ((dir = strrchr(path, '/'))) 
        {
            *dir = '\0';
            result = mkdirr(path, mode,fail_on_exist);
            *dir = '/';
            if (result)
                break;
        }
        if (strlen(path))
        {
            if ((result = mkdir(path, mode)))
            {
                if(EEXIST==errno && fail_on_exist==0)
                    result=0;
            }
        }
    } while (0);
    if(path)
        free(path);
    return result;
}

void serv_creat(int client_sock)
{
    char* fileName = clientMessage(client_sock);
    // read(client_sock,fileName,siz);
    //read siz amount of bytes for filename
    if(fileLookup(fileName))
        write(client_sock,"1",1);
    else
    {
        //send confirmation
        write(client_sock,"0",1);
        char* path = myMalloc(strlen(fileName)+13);
        sprintf(path,"./%s",fileName);
        mkdir(path,00777);
        strcat(path,"/.Manifest");
        int fd = open(path, O_WRONLY | O_CREAT,00600);
        if(fd<0)
        {
            printf("Could not create project %s\n",fileName);
            free(fileName);
            free(path);
            return;
        }
        write(fd,"0\n",2);
        char* comm = myMalloc(strlen(fileName)+8);
        sprintf(comm,"%s/commit",fileName);
        mkdir(comm,00777);
        free(comm);
        char* data = myMalloc(strlen(fileName)+7);
        sprintf(data,"%s/.data",fileName);
        mkdir(data,00777);
        free(data);
        char* hist = myMalloc(strlen(fileName)+5);
        sprintf(hist,"%s/.data",fileName);
        fd = open(hist,O_WRONLY|O_CREAT,00777);
        write(fd,"0\n",2);
        free(path);
        close(fd);
    }
	mutex_creat(fileName);
    free(fileName);
}

int recurse_del(char* file)
{
    struct dirent* dp;
    DIR* dir = opendir(file);
    dp = readdir(dir);
    dp = readdir(dir);
    dp = readdir(dir);
    if(dp == NULL)
    {
        free(dir);
        return 1;
    }
    char* temp;
    while(dp!=NULL)
    {
        if(dp->d_type == 4)
        {
            temp = myMalloc(strlen(file)+256);
            sprintf(temp,"%s/%s",file,dp->d_name);
            if(recurse_del(temp))
                remove(temp);
            free(temp);
        }
        else if(dp->d_type == 8)
        {
            temp = myMalloc(strlen(file)+256);
            sprintf(temp,"%s/%s",file,dp->d_name);
            remove(temp);
            free(temp);
        }
        dp = readdir(dir);
    }
    free(dir);
    return 1;
}

void serv_del(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    if(fileLookup(fileName))
    {
        if(recurse_del(fileName))
            remove(fileName);
        write(client_sock,"1",1); 
    }
    else
        write(client_sock,"0",1);
    free(fileName);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
}

manifest* serv_ver(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    if(fileLookup(fileName))
    {
        char* buf = myMalloc(11+strlen(fileName));
        sprintf(buf,"%s/.Manifest",fileName);
        int fd = open(buf,O_RDONLY);
        if(fd<0)
        {
            printf("Error: Could not find .Manifest for %s\n",fileName);
            write(client_sock,"2",1);
            free(fileName);
            free(buf);
            close(fd);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return NULL;
        }
        manifest* man = loadManifest(buf);
        free(buf);
        if(man==NULL)
        {
            printf("Error: .Manifest is empty\n");
            write(client_sock,"3",1);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return NULL;
        }
        write(client_sock,"1",1);
        char* ver = myMalloc(16);
        sprintf(ver,"%d\n",man->version);
        write(client_sock,ver,strlen(ver));
        free(ver);
        file* temp;
        int i = 0;
        for(i;i<20;i++)
        {
            temp = man->files[i];
            if(temp!=NULL)
            {
                char* line = myMalloc(12 + strlen(temp->filename)+strlen(temp->digest));
                sprintf(line,"%d\t%s\t%s\n",temp->version,temp->filename,temp->digest);
                write(client_sock,line,strlen(line));
                free(line);
                temp = temp->next;
            }
        }
		if(project != NULL)
			pthread_mutex_unlock(project->lock);
        return man;
    }
    else
        write(client_sock,"0",1);
    free(fileName);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
    return NULL;
}

void writeFile(char* file, int client_sock)
{
    int fd = open(file,O_RDONLY);
    write(client_sock,file,strlen(file));
    write(client_sock,"\t",1);
    char* c = myMalloc(1);
    while(read(fd,c,1)>0)
        write(client_sock,c,1);
}

void serv_check(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    if(!fileLookup(fileName))
    {
        printf("Error: Project %s does not exist.\n",fileName);
        write(client_sock,"0",1);
        free(fileName);
		if(project != NULL)
			pthread_mutex_unlock(project->lock);
        return;
    }
    char* manPath = myMalloc(strlen(fileName)+11);
    sprintf(manPath,"%s/.Manifest",fileName);
    manifest* man = loadManifest(manPath);
    if(man == NULL)
    {
        write(client_sock,"2",1);
        free(fileName);
        free(manPath);
		if(project != NULL)
			pthread_mutex_unlock(project->lock);
        return;
    }
    write(client_sock,"1",1);
    char* ver = myMalloc(16);
    sprintf(ver,"%d\n",man->version);
    write(client_sock,ver,strlen(ver));
    free(ver);
    file* temp;
    int i = 0;
    for(i;i<20;i++)
    {
        temp = man->files[i];
        if(temp!=NULL)
        {
            char* line = myMalloc(12 + strlen(temp->filename)+strlen(temp->digest));
            sprintf(line,"%d\t%s\t%s\n",temp->version,temp->filename,temp->digest);
            write(client_sock,line,strlen(line));
            free(line);
            temp = temp->next;
        }
    }
    write(client_sock,"~\t",2);
    printf("Finished with Manifest, sending files...\n");
    i = 0;
    for(i;i<20;i++)
    {
        temp = man->files[i];
        while(temp!=NULL)
        {
            int fd = open(temp->filename,O_RDONLY);
            if(fd<0)
                printf("Error: Could not find file %s",temp->filename);
            else
                writeFile(temp->filename,client_sock);
            close(fd);
            temp = temp->next;
        }
    }
    free(man);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
}

void serv_upgrade(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    if(!fileLookup(fileName))
    {
        printf("Error: Project %s does not exist.\n",fileName);
        write(client_sock,"0",1);
        free(fileName);
		if(project != NULL)
			pthread_mutex_unlock(project->lock);
        return;
    }
    char* manPath = myMalloc(strlen(fileName)+11);
    sprintf(manPath,"%s/.Manifest",fileName);
    manifest* man = loadManifest(manPath);
    if(man == NULL)
    {
        write(client_sock,"2",1);
        free(fileName);
        free(manPath);
		if(project != NULL)
			pthread_mutex_unlock(project->lock);
        return;
    }
    //manifest exists
    write(client_sock,"1",1);
    //write version
    char* ver = myMalloc(16);
    sprintf(ver,"%d\n",man->version);
    write(client_sock,ver,strlen(ver));
    free(ver);
    //write out all files from manifest
    file* temp;
    int i = 0;
    for(i;i<20;i++)
    {
        temp = man->files[i];
        while(temp!=NULL)
        {
            int fd = open(temp->filename,O_RDONLY);
            if(fd<0)
                printf("Error: Could not find file %s",temp->filename);
            else
                writeFile(temp->filename,client_sock);
            close(fd);
            temp = temp->next;
        }
    }
    free(man);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
}

node* readSocket(int socket)
{
    char* c = myMalloc(sizeof(char));
    node* head = NULL;                  //linked list of chars to create a token
    node* temp;
    node* list = NULL;                  //linked list of tokens from file
    length = 0;
    while(read(socket,c,1) > 0)
    {
        if(*c != '\t' && *c != '\n')
        {
            length+=1;
            temp = initNode();
            temp->data = myMalloc(sizeof(char));
            memcpy(temp->data,c,1);
            head = insert(head,temp);
        }
        else
        {
            list = addToList(list,head);
            length=0;
            freeList(head);
            if(*c == '\t')
            {
                length = 1;
                temp = initNode();
                temp->data = myMalloc(1);
                memcpy(temp->data,"\t",1);
                list = addToList(list,temp);
                length = 0;
            }
            else if(*c == '\n')
            {
                length = 1;
                temp = initNode();
                temp->data = myMalloc(1);
                memcpy(temp->data,"\n",1);
                list = addToList(list,temp);
                length = 0;
            }
            head = NULL;
        }
    }
    if(length > 0)
        list = addToList(list,head);
    freeList(head);
    if(list == NULL)
    {
        printf("Error: socket is empty\n");
        return NULL;
    }
    free(c);
    return list;
}

node* readSocketL(int socket,int size)
{
    char* c = myMalloc(sizeof(char));
    int i = 0;
    node* head = NULL;                  //linked list of chars to create a token
    node* temp;
    node* list = NULL;                  //linked list of tokens from file
    length = 0;
    while(read(socket,c,1) > 0)
    {
        if(*c != '\t' && *c != '\n')
        {
            length+=1;
            temp = initNode();
            temp->data = myMalloc(sizeof(char));
            memcpy(temp->data,c,1);
            head = insert(head,temp);
        }
        else
        {
            list = addToList(list,head);
            length=0;
            freeList(head);
            if(*c == '\t')
            {
                length = 1;
                temp = initNode();
                temp->data = myMalloc(1);
                memcpy(temp->data,"\t",1);
                list = addToList(list,temp);
                length = 0;
            }
            else if(*c == '\n')
            {
                length = 1;
                temp = initNode();
                temp->data = myMalloc(1);
                memcpy(temp->data,"\n",1);
                list = addToList(list,temp);
                length = 0;
            }
            head = NULL;
        }
        i++;
        if(i==size)
            break;
    }
    if(length > 0)
        list = addToList(list,head);
    freeList(head);
    if(list == NULL)
    {
        printf("Error: socket is empty\n");
        return NULL;
    }
    free(c);
    return list;
}

void serv_commit(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    if(fileLookup(fileName))
    {
        char* buf = myMalloc(11+strlen(fileName));
        sprintf(buf,"%s/.Manifest",fileName);
        int fd = open(buf,O_RDONLY);
        if(fd<0)
        {
            printf("Error: Could not find .Manifest for %s\n",fileName);
            write(client_sock,"2",1);
            free(fileName);
            free(buf);
            close(fd);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        manifest* man = loadManifest(buf);
        free(buf);
        if(man==NULL)
        {
            printf("Error: .Manifest is empty\n");
            write(client_sock,"3",1);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        write(client_sock,"1",1);
        int msg = 0;
        //calculate msg length
        char* ver = myMalloc(16);
        sprintf(ver,"%d\n",man->version);
        msg+=strlen(ver);
        file* temp;
        int i = 0;
        for(i;i<20;i++)
        {
            temp = man->files[i];
            while(temp!=NULL)
            {
                char* line = myMalloc(12 + strlen(temp->filename)+strlen(temp->digest));
                sprintf(line,"%d\t%s\t%s\n",temp->version,temp->filename,temp->digest);
                msg+=strlen(line);
                free(line);
                temp = temp->next;
            }
        }
        char* msgsize = myMalloc(100);
        sprintf(msgsize,"%d~",msg);
        write(client_sock,msgsize,strlen(msgsize));
        //actually write manifest out
        write(client_sock,ver,strlen(ver));
        free(ver);
        i = 0;
        for(i;i<20;i++)
        {
            temp = man->files[i];
            while(temp!=NULL)
            {
                char* line = myMalloc(12 + strlen(temp->filename)+strlen(temp->digest));
                sprintf(line,"%d\t%s\t%s\n",temp->version,temp->filename,temp->digest);
                write(client_sock,line,strlen(line));
                free(line);
                temp = temp->next;
            }
        }
        char* ans = myMalloc(1);
        read(client_sock,ans,1);
        if(atoi(ans)==1)
        {
            node* list = readSocket(client_sock);
            if(list==NULL)
            {
                printf("Did not receive commit from client\n");
				if(project != NULL)
					pthread_mutex_unlock(project->lock);
                return;
            }
            node* ptr = list->next;
            char* comfile = myMalloc(strlen(list->data)+strlen(fileName)+9);
            sprintf(comfile,"%s/commit/%s",fileName,list->data);
            fd = open(comfile,O_WRONLY|O_APPEND);
            if(fd>0)
			{
				if(project != NULL)
					pthread_mutex_unlock(project->lock);
                return;
			}
            fd = open(comfile,O_WRONLY|O_APPEND|O_CREAT,00777);
            ptr = ptr->next;
            while(ptr!=NULL)
            {
                write(fd,ptr->data,strlen(ptr->data));
                ptr = ptr->next;
            }
            freeList(list);
            free(comfile);
            close(fd);
        }
    }
    else
        write(client_sock,"0",1);
    free(fileName);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
    return;
}

void expire(char* project)
{
    char* commit = myMalloc(strlen(project)+10);
    sprintf(commit,"%s/commit",project);
    //open commit folder
    DIR* dir = opendir(commit);
    struct dirent* dp;
    if(dir!=NULL)
    {
        dp = readdir(dir);
        dp = readdir(dir);
        dp = readdir(dir);
        //delete everything all commits even the one given because it is already loaded
        while(dp!=NULL)
        {
            char* del = myMalloc(strlen(commit)+50);
            sprintf(del,"%s/%s",commit,dp->d_name);
            remove(del);
            free(del);
            dp = readdir(dir);
        }
    }
    else
    {
        printf("Error: There is no commit folder for this project, please create one\n");
        return;
    }
}

void serv_push(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    if(fileLookup(fileName))
    {
        //tell client project exists
        write(client_sock,"1",1);
        //find how long files will be
        char* buff = myMalloc(100);
        char* rd = myMalloc(1);
        int c = 0;
        while(read(client_sock,rd,1)>0)
        {
            if(*rd == '~')
                break;
            memcpy(buff+c,rd,1);
            c++;
        }
        int bytes = atoi(buff);
        node* files = readSocketL(client_sock,bytes);
        if(files==NULL)
        {
            printf("Error: Did not receive .Commit");
            free(fileName);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        //check for already existing commit
        char* com = myMalloc(strlen(files->data)+1);
        memcpy(com,files->data,strlen(files->data));
        int fd = open(com,O_RDONLY);
        if(fd<0)
        {
            printf("Error: Could not find a similar .Commit, client must run commit first\n");
            write(client_sock,"4",1);
            free(com);
            free(fileName);
            freeList(files);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        //load up commit here to avoid copying to old 
        node* commit = readFile(com);
        if(commit == NULL)
        {
            printf("Error: Something went wrong reading from commit\n");
            free(com);
            free(fileName);
            free(files);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        //expire all other commits
        expire(fileName);
        //check if manifest exists
        char* manPath = myMalloc(strlen(fileName)+11);
        sprintf(manPath,"%s/.Manifest",fileName);
        fd = open(manPath,O_RDONLY);
        if(fd<0)
        {
            write(client_sock,"2",1);
            free(manPath);
            free(com);
            freeList(files);
            free(commit);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        manifest* man = loadManifest(manPath);
        if(man==NULL)
        {
            write(client_sock,"3",1);
            free(manPath);
            free(com);
            freeList(files);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        int failure = 0;
        //make temp folder to hold old version
        char* dupe = myMalloc(strlen(fileName)+5);
        sprintf(dupe,"%stemp",fileName);
        if(mkdir(dupe,00777)<0)
        {
            printf("Error: Could not create a duplicate directory");
            write(client_sock,"1",1);
            freeList(files);
            free(dupe);
            free(com);
            close(fd);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        char* cpy = myMalloc((2*strlen(fileName))+20);
        //copy to this temp folder
        sprintf(cpy,"cp -r %s %stemp",fileName,fileName);
        system(cpy);
        free(cpy);
        //traverse commit file
        node* cptr = commit;
        while(cptr!=NULL)
        {
            if(strcmp(cptr->data,"A")==0)
            {
                file* temp = initFile();
                cptr= cptr->next;
                temp->version = atoi(cptr->data);
                cptr = cptr->next;
                temp->filename = myMalloc(strlen(cptr->data)+1);
                memcpy(temp->filename,cptr->data,strlen(cptr->data));
                cptr = cptr->next;
                temp->digest = myMalloc(strlen(cptr->data)+1);
                memcpy(temp->digest,cptr->data,strlen(cptr->data));
                man = insertToManifest(man,temp);
                //no guarantee path exists, create it
                char* path = myMalloc(strlen(temp->filename)+1);
                memcpy(path,temp->filename,strlen(temp->filename));
                char* cut = strrchr(path,'/');
                *cut = '\0';
                if(mkdirr(path,00777,0) == -1)
                {
                    printf("Could not create path %s",path);
                    failure = 1;
                }
                else
                {
                    *cut = '/';
                    //create file
                    int wfd = open(path,O_WRONLY|O_APPEND|O_CREAT,00777);
                    if(wfd<0)
                    {
                        printf("Error: Could not create file according to path %s",path);
                        failure = 1;
                    }
                    //skip commit hash
                    node* fptr = files->next;
                    //fill with new data
                    while(fptr!=NULL)
                    {
                        if(strcmp(fptr->data, temp->filename)==0)
                        {
                            //skip the tab after filenames
                            fptr = fptr->next;
                            fptr = fptr->next;
                            while(fd=open(fptr->data,O_WRONLY|O_APPEND)<0)
                            {
                                write(wfd,fptr->data,strlen(fptr->data));
                                fptr = fptr->next;
                                if(fptr==NULL)
                                    break;
                            }
                        }
                        if(fptr==NULL)
                            break;
                        fptr=fptr->next;
                    }
                }
            }
            else if(strcmp(cptr->data,"M")==0)
            {
                //create the new file
                file* temp = initFile();
                cptr = cptr->next;
                temp->version = atoi(cptr->data);
                cptr=cptr->next;
                temp->filename = myMalloc(strlen(cptr->data)+1);
                memcpy(temp->filename,cptr->data,strlen(cptr->data));
                cptr=cptr->next;
                temp->digest = myMalloc(strlen(cptr->data)+1);
                memcpy(temp->digest,cptr->data,strlen(cptr->data));
                //remove local version
                remove(temp->filename);
                //remove manifest entry
                man = deleteFromManifest(man,temp->filename);
                //replace manifest entry
                man = insertToManifest(man,temp);
                //create empty new version
                int wfd = open(temp->filename,O_WRONLY|O_APPEND|O_CREAT,00777);
                if(wfd<0)
                {
                    printf("Error: Could not create file according to path %s",temp->filename);
                    failure = 1;
                }
                node* fptr = files;
                //fill with new data
                while(fptr!=NULL)
                {
                    if(strcmp(fptr->data, temp->filename)==0)
                    {
                        fptr = fptr->next;
                        fptr = fptr->next;
                        while(fd=open(fptr->data,O_WRONLY|O_APPEND)<0)
                        {
                            write(wfd,fptr->data,strlen(fptr->data));
                            fptr = fptr->next;
                            if(fptr==NULL)
                                break;
                        }
                    }
                    if(fptr==NULL)
                        break;
                    fptr=fptr->next;
                }
            }
            else if(strcmp(cptr->data,"D")==0)
            {
                cptr = cptr->next;
                cptr = cptr->next;
                man = deleteFromManifest(man,cptr->data);
                char* path = myMalloc(strlen(cptr->data)+1);
                memcpy(path,cptr->data,strlen(cptr->data));
                if(remove(path)<0)
                    failure = 1;
            }
            cptr = cptr->next;
        }
        if(failure)
        {
            remove(fileName);
            char* move = myMalloc((2*strlen(fileName))+50);
            sprintf(move,"mv %stemp/%s .",fileName,fileName);
            system(move);
            free(move);
            remove(dupe);
            write(client_sock,"1",1);
        }
        else
        {
            //move the old version into .data
            char* rename = myMalloc((2*strlen(fileName))+50);
            sprintf(rename,"mv %stemp/%s %s/.data/%s%d",fileName,fileName,fileName,fileName,man->version);
            system(rename);
            free(rename);
            remove(dupe);
            //write the commits into history
            char* his = myMalloc(strlen(fileName)+10);
            sprintf(his,"%s/.history",fileName);
            int wfd = open(his,O_WRONLY|O_APPEND);
            free(his);
            char* version = myMalloc(16);
            sprintf(version,"%d\n",man->version);
            write(wfd,version,strlen(version));
            cptr = commit;
            int f = 1;
            while(cptr!=NULL)
            {
                write(wfd,cptr->data,strlen(cptr->data));
                if(f%4==0)
                {
                    write(wfd,"\n",1);
                    f-=3;
                }
                else
                {
                    write(wfd,"\t",1);
                    f++;
                }
                cptr = cptr->next;
            }
            //separate by newline
            write(wfd,"\n",1);
            close(wfd);
            //increase manifest version#
            man->version+=1;
            //replace manifest
            createManifestFile(man,manPath);
            write(client_sock,"0",1);
        }
        freeList(files);
        freeList(commit);
        free(manPath);
        freeManifest(man);
    }
    else
        write(client_sock,"0",1);
    free(fileName);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
    return;
}

void serv_history(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    //check if project exists
    if(fileLookup(fileName))
    {
        //find history path
        char* hist = myMalloc(strlen(fileName)+10);
        sprintf(hist,"%s/.history",fileName);
        int fd = open(hist,O_RDONLY);
        //attempt to open history
        if(fd<0)
        {
            write(client_sock,"2",1);
            printf("Error: Could not find .history for %s\n",fileName);
			if(project != NULL)
				pthread_mutex_unlock(project->lock);
            return;
        }
        //tell client it exists and will be sent
        write(client_sock,"1",1);
        //send .history
        writeFile(hist,client_sock);
    }
    else
        write(client_sock,"0",1);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
    return;
}

void serv_rollback(int client_sock)
{
    char* fileName = clientMessage(client_sock);
	mutex* project = getMutex(fileName);
	if(project != NULL)
		pthread_mutex_lock(project->lock);
    if(fileLookup(fileName))
    {
        //let client know project exists and to send version #
        write(client_sock,"1",1);
        char* version = myMalloc(16);
        char* ans = myMalloc(1);
        int c = 0;
        while(read(client_sock,ans,1)>0)
        {
            if(*ans == '~')
                break;
            memcpy(version+c,ans,1);
            c++;
        }
        //path is project/.data/project<version>
        char* looking = myMalloc(11+strlen(version)+(2*strlen(fileName)));
        sprintf(looking,"%s/.data/%s%s",fileName,fileName,version);
        DIR* dir = opendir(looking);
        if(dir!=NULL)
        {
            //cpy to cwd
            char* cpy = myMalloc(strlen(looking)+20);
            sprintf(cpy,"cp -r %s/.data/%s%s .",fileName,fileName,version);
            system(cpy);
            //delete the directory recursively
            if(recurse_del(fileName))
                remove(fileName);
            //rename the older version
            char* rename = myMalloc(strlen(looking)+20);
            sprintf(rename,"mv %s%s %s",fileName,version,fileName);
            system(rename);
            free(cpy);
            free(rename);
            write(client_sock,"1",1);
        }
        //version # not there
        else
            write(client_sock,"0",1);
    }
    else 
        write(client_sock,"0",1);
	if(project != NULL)
		pthread_mutex_unlock(project->lock);
    return;
}

void* handle_connection(void* cs)
{
	int client_sock = *((int*)cs);
    char* flag = myMalloc(3);
    int i = 0;
    char* c = myMalloc(1);
    //get the flag
    while(read(client_sock,c,1) > 0 && *c != '~')
        strcat(flag,c);
    if(strcmp(flag,"CRT")==0)
	{
        serv_creat(client_sock);
    }else if(strcmp(flag,"DES")==0)
        serv_del(client_sock);
    else if(strcmp(flag,"VER")==0 || strcmp(flag,"UPD")==0)
    {
        manifest* man = serv_ver(client_sock);
        if(man!=NULL)
            free(man);
    }
    else if(strcmp(flag,"CHK")==0)
        serv_check(client_sock);
    else if(strcmp(flag,"UPG")==0)
        serv_upgrade(client_sock);
    else if(strcmp(flag,"COM")==0)
        serv_commit(client_sock);
    else if(strcmp(flag,"PSH")==0)
        serv_push(client_sock);
    else if(strcmp(flag,"HIS")==0)
        serv_history(client_sock);
    else if(strcmp(flag,"ROL")==0)
        serv_rollback(client_sock);
    free(c);
    free(flag);
    close(client_sock);
    printf("Server: Client has disconnected.\n");
}
void exiting()
{
	close(serv_sock);
	freeMutexes();	
	printf("\nServer: terminated\n");
	
}

void sigHandler(int signum)
{
	exit(3);
}

int main(int argc, char** argv)
{
    if(argc!=2)
    {
        printf("Error: expected 2 arguments, received %d\n",argc);
        return 0;
    }
    int port = atoi(argv[1]);
    if(port <= 1024)
    {
        printf("Error: %s is not a valid port\n",argv[1]);
        return 0;
    }
    int serv_sock;
    serv_sock = socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    setsockopt(serv_sock,SOL_SOCKET,SO_REUSEPORT,&opt,sizeof(opt));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int b = bind(serv_sock,(struct sockaddr*)&server_addr,sizeof(server_addr));
    if(b<0)
    {
        perror("Bind failed: ");
        return 0;
    }
    int l = listen(serv_sock,1);
    if(l<0)
    {
        perror("Listen failed: ");
        return 0;
    }
    int client_sock;
	signal(SIGINT, sigHandler);
	atexit(exiting);
	while(1)
	{
	    client_sock = accept(serv_sock,NULL,NULL);
		if(client_sock > 0)
		{
			printf("Server: New client accepted\n");
			pthread_t tid;
   			pthread_create(&tid, NULL, handle_connection, &client_sock);
		}else
			printf("Server: Error: Client could not be accepted.\n");
	}
    return 0;
}
