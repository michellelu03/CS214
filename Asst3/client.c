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
#include <openssl/sha.h>
#include <errno.h>

int length;

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
	file** files;
}manifest;

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

node* initNode()
{
    node* temp = myMalloc(sizeof(node));
    temp->next = NULL;
    temp->data = NULL;
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

void check(int num, char* msg)
{
    if(num < 0)
    {
        printf("%s\n",msg);
        exit(1);
    }
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
	close(fd);
	return hash;
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

manifest* initManifest()
{
	manifest* temp = myMalloc(sizeof(manifest));
	temp->version = 0;
	temp->files = myMalloc(20*sizeof(file*));
	return temp;
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

manifest* insertToManifest(manifest* m, file* f)
{
	int bucket = (getASCII(f->filename)) % 20;
	f->next = m->files[bucket]; 
	m->files[bucket] = f;
	return m;
}

manifest* updateManifest(manifest* m, char* filename)
{
	int bucket = (getASCII(filename)) % 20;
	file* ptr;
	for(ptr = m->files[bucket]; ptr != NULL; ptr = ptr->next){
		if(strcmp(ptr->filename, filename) == 0)
		{
			ptr->version++;
			ptr->digest = getDigest(filename);
			break;
		}
	}
	return m;
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

void config(char* ip,char* port)
{
    //Override previous config file
    remove(".configure");
    //check valid ip length
    if(strlen(ip) > 255 || strlen(ip) == 0)
    {
        printf("Error: ip adress is not valid. Aborting\n");
        exit(1);
    }
    //check valid port
    if(atoi(port) == 0)
    {
        printf("Error: port must be an integer. Aborting\n");
        exit(1);
    }
    else
    {
        if(atoi(port) > 65535 || atoi(port) < 1024)
        {
            printf("Error: port must be between 1024 and 65535\n");
            exit(1);
        }
    }
    //create file
    int wfd = open(".configure", O_WRONLY | O_APPEND | O_CREAT,00600);
    check(wfd,"Error: Could not create file in this directory");
    //write input to file
    char* buf = myMalloc(strlen(ip)+strlen(port)+1);
    sprintf(buf,"%s\t%s",ip,port);
    write(wfd,buf,strlen(buf));
    printf("Client: .Configure file created with IP address/hostname and port number.\n");
    free(buf);
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

node* readFile(char* file)
{
    int fd = open(file,O_RDONLY);
    if(fd<0)
        return NULL;
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
        close(fd);
        return NULL;
    }
    free(c);
    return list;
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
        exit(1);
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
        exit(1);
    }
    if(c)
        free(c);
    return list;
}

int initSocket()
{
    node* list = readFile(".configure");
    if(list == NULL)
    {
        printf("Error: .configure is either empty, or could not be read. Aborting.\n");
        exit(1);
    }
    node* temp = list->next;
    if(temp == NULL)
    {
        printf("Error: .configure file is not formatted properly. Aborting.\n");
        exit(1);
    }
    char* ip = myMalloc(256);
    memcpy(ip,list->data,strlen(list->data));
    char* port = myMalloc(6);
    memcpy(port,temp->data,strlen(temp->data));
    int port_num = atoi(port);
    //init socket
    int sock = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in server;
    bzero((char*)&server,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port_num);
    if(strcmp(ip,"INADDR_ANY")==0)
        server.sin_addr.s_addr = INADDR_ANY;
    else
    {
        struct hostent* host = gethostbyname(ip);
        if(host == NULL)
        {
            printf("Error: No such host\n");
            return -1;
        }
        bcopy((char*)host->h_addr,(char*)&server.sin_addr.s_addr,host->h_length);
    }
    free(ip);
    free(port);
    freeList(list);
    int status = connect(sock,(struct sockaddr*)&server,sizeof(server));
    if(status<0)
    { 
        char* error = myMalloc(256);
        sprintf(error,"Could not connect to server:");
        perror(error);
        free(error);
        exit(1);
    }
    printf("Client: successfully connected to server.\n");
    return sock;
}

void client_creat(char* file,int sock)
{
    //max file name length is 255, need 4 for create flag, need 4 max for number of bytes
    char* buf = myMalloc(300);
    char* ans = myMalloc(1);
    //construct a message in the format CRT~#bytes~filename
    sprintf(buf,"CRT~%d~%s",strlen(file),file);
    write(sock,buf,strlen(buf));
    //printf("Finished writing to socket\n");
    read(sock,ans,1);
    int a = atoi(ans);
    if(a == 0)
    {
        printf("Client: Project %s was created.\n",file);
        mkdir(file,00777);
        sprintf(buf,"%s/.Manifest",file);
        int fd = open(buf, O_WRONLY|O_APPEND|O_CREAT,00777);
        check(fd,"Error: Could not create a .Manifest for this project.");
        write(fd,"0\n",2);
        close(fd);
    }
    else
        printf("The project already exists.\n");
    free(buf);
    free(ans);
    return;
}

void client_des(char* file,int sock)
{
    //max message length
    char* buf = myMalloc(300);
    char* ans = myMalloc(1);
    sprintf(buf,"DES~%d~%s",strlen(file),file);
    write(sock,buf,strlen(buf));
    // printf("Finished writing to socket\n");
    //read response
    read(sock,ans,1);
    if(atoi(ans) == 0)
        printf("Error: Project %s does not exist.\n",file);
    else
        printf("Client: Project %s has been destroyed.\n",file);
    free(buf);
    free(ans);
    return;
}

void add(char* project,char* file)
{
    struct dirent* dp;
    DIR* dir = opendir(project);
    //check if project exists
    if(dir == NULL)
    {
        printf("Error: The project %s does not exist.\n");
        return;
    }
    dp = readdir(dir);
    dp = readdir(dir);
    dp = readdir(dir);
    //check if project has files
    if(dp==NULL)
    {
        printf("Error: The project %s is empty. Aborting\n",project);
        free(dir);
        return;
    }
    char* path = myMalloc(strlen(project)+strlen(file)+1);
    char* man = myMalloc(strlen(project)+11);
    sprintf(path,"%s/%s",project,file);
    int fd = open(path,O_RDONLY);
    //check if file exists
    check(fd,"Error: The file or path provided does not exist. Aborting.");
    sprintf(man,"%s/.Manifest",project);
    while(dp!=NULL)
    {
        if(strcmp(dp->d_name,".Manifest")==0)
        {
            node* list = readFile(man);
            if(list==NULL)
            {
                printf("Error: .Manifest is empty\n");
                return;
            }
            node* ptr = list;
            while(ptr != NULL)
            {
                if(strcmp(ptr->data,path)==0)
                {
                    printf("Warning: There is already a file: %s inside of %s, please remove it first.\n",file,project);
                    free(path);
                    free(man);
                    free(dir);
                    freeList(list);
                    return;
                }
                ptr = ptr->next;
            }
            fd = open(man, O_WRONLY | O_APPEND);
            check(fd,"Error: Could not find .Manifest, Aborting.");
            char* hash = getDigest(path);
            char* buf = myMalloc(5+strlen(project)+strlen(file)+strlen(hash));
            sprintf(buf,"0\t%s/%s\t%s\n",project,file,hash);
            write(fd,buf,strlen(buf));
            free(hash);
            free(path);
            free(man);
            free(dir);
            freeList(list);
            free(buf);
	    printf("Client: file added successfully.\n");
            return;
        }
        dp = readdir(dir);
    }
}

manifest* loadManifest(char* manPath)
{
    node* list = readFile(manPath);
    if(list == NULL)
        return NULL;
    manifest* m = initManifest();
    m->version = atoi(list->data);
    node* ver = list;
    list = list->next;
    free(ver);
    node* ptr = list;
    file* temp = initFile();
    int i = 1;
    while(ptr!=NULL)
    {
        if(i%3==0)
        {
            temp->digest = myMalloc(strlen(ptr->data)+1);
            memcpy(temp->digest,ptr->data,strlen(ptr->data)+1);
            insertToManifest(m,temp);
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

void rem(char* project,char* file)
{
    struct dirent* dp;
    DIR* dir = opendir(project);
    //check if project exists
    if(dir==NULL)
    {
        printf("Error: The project %s does not exist.\n",project);
        return;
    }
    dp = readdir(dir);
    dp = readdir(dir);
    dp = readdir(dir);
    //check if project has at least one file
    if(dp == NULL)
    {
        printf("Error: The project %s is empty.\n",project);
        free(dir);
        return;
    }
    //path to file
    char* path = myMalloc(strlen(project)+strlen(file)+1);
    sprintf(path,"%s/%s",project,file);
    //path to .Manifest
    char* manPath = myMalloc(strlen(project) + 11);
    sprintf(manPath,"%s/.Manifest",project);
    int fd = open(manPath,O_RDONLY);
    if(fd<0)
    {
        printf("Error: There is no .Manifest in this project\n");
        free(dir);
        free(path);
        free(manPath);
        return;
    }
    close(fd);
    //load old manifest
    manifest* man = loadManifest(manPath);
    if(man==NULL)
    {
        printf("Error: Something went wrong while loading .Manifest\n");
        free(dir);
        free(path);
        free(manPath);
        return;
    }
    //delete corresponding file
    man = deleteFromManifest(man,path);
    //recreate manifest without file
    createManifestFile(man,manPath);
    freeManifest(man);
    free(path);
    free(manPath);
    free(dir);
    printf("Client: file removed successfully.\n");
    return;
}

void client_ver(char* project, int sock)
{
    //allocate enough for a message
    char* buf = myMalloc(300);
    char* ans = myMalloc(1);
    sprintf(buf,"VER~%d~%s",strlen(project),project);
    write(sock,buf,strlen(buf));
    free(buf);
    printf("Finished writing to socket\n");
    read(sock,ans,1);
    if(atoi(ans)==1)
    {
        node* list = readSocket(sock);
        node* ptr = list;
        if(list == NULL)
        {
            printf("Error: Something went wrong reading from socket\n");
            free(ans);
            exit(1);
        }
        printf("%s\n",ptr->data);
        ptr = ptr->next;
        int i = 1;
        while(ptr!=NULL)
        {
            if(i%3==0)
            {
                printf("%s\n",ptr->data);
                i-=2;
            }
            else
            {
                printf("%s\t",ptr->data);
                i++;
            }
            ptr=ptr->next;
        }
        freeList(list);
	printf("Client: version command successful.\n");
    }
    else if(atoi(ans)==2)
        printf("Error: There was no .Manifest in %s\n",project);
    else
        printf("Error: There is no project named %s\n",project);
    free(ans);
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

void client_check(char* project,int sock)
{
    struct dirent* dp;
    DIR* dir = opendir(project);
    //check if project exists
    if(dir==NULL)
    {
        char* buf = myMalloc(300);
        char* ans = myMalloc(1);
        sprintf(buf,"CHK~%d~%s",strlen(project),project);
        write(sock,buf,strlen(buf));
        free(buf);
        read(sock,ans,1);
        if(atoi(ans)==1)
        {
            node* list = readSocket(sock);
            if(list == NULL)
            {
                printf("Error: Could not read from socket\n");
                free(ans);
                return;
            }
            //make project folder
            mkdir(project,00777);
            char* manPath = myMalloc(strlen(project)+11);
            sprintf(manPath,"%s/.Manifest",project);
            int fd = open(manPath,O_WRONLY|O_APPEND|O_CREAT,00777);
            if(fd<0)
            {
                printf("Error: Could not create a .Manifest");
                return;
            }
            node* ptr = list;
            //write version 
            char* ver = myMalloc(16);
            sprintf(ver,"%s\n",ptr->data);
            write(fd,ver,strlen(ver));
            free(ver);
            ptr = ptr->next;
            int i = 1;
            char* line = myMalloc(256);
            while(ptr!=NULL)
            {
                if(strcmp(ptr->data,"~")==0)
                    break;
                if(strcmp(ptr->data,"\t")!=0 && strcmp(ptr->data,"\n")!=0)
                {
                    if(i%3==0)
                    {
                        strcat(line,ptr->data);
                        strcat(line,"\n");
                        write(fd,line,strlen(line));
                        free(line);
                        line = myMalloc(256);
                        i-=2;
                    }
                    else
                    {
                        strcat(line,ptr->data);
                        strcat(line,"\t");
                        if(i%2==0)
                        {
                            char* path = myMalloc(strlen(ptr->data)+1);
                            memcpy(path,ptr->data,strlen(ptr->data));
                            char* cut = strrchr(path,'/');
                            *cut = '\0';
                            if(mkdirr(path,00777,0) == -1)
                                printf("Could not create path %s",path);
                            else
                            {
                                *cut = '/';
                                int wfd = open(path,O_CREAT,00777);
                                close(wfd);
                            }
                        }
                        i++;
                    }
                }
                ptr=ptr->next;
            }
            close(fd);
            //move past delimiter
            ptr = ptr->next;
            //start reading the file data after initializing all directories
            int wfd;
            while(ptr!=NULL)
            {
                if((fd=open(ptr->data,O_WRONLY|O_APPEND))<0)
                    write(wfd,ptr->data,strlen(ptr->data));
                else
                {
                    wfd = fd;
                    ptr = ptr->next;
                }
                ptr = ptr->next;
            }
            close(fd);
            close(wfd);
	    printf("Client: checkout command successful.\n");
            if(list!=NULL)
                freeList(list);
        }
        else if(atoi(ans)==2)
            printf("Error: Could not find a .Manifest for project %s. Aborting\n");
        else
            printf("Error: The project %s does not exist on the server.\n",project);
        free(ans);
    }
    else
        printf("Error: The project %s exists locally, cannot checkout.\n",project);
    return;
}

int serverDifference(char* project, manifest* client, manifest* server)
{
    char* up = myMalloc(strlen(project)+9);
    sprintf(up,"%s/.Update",project);
    int wfd = open(up,O_WRONLY|O_APPEND|O_CREAT,00777);
    if(wfd<0)
    {
        printf("Error: Could not create an Update file\n");
        free(up);
        close(wfd);
        return;
    }
    if(client->version == server->version)
    {
        char* conflict = myMalloc(strlen(project)+11);
        sprintf(conflict,"%s/.Conflict",project);
        remove(conflict);
        printf("Up To Date\n");
        free(up);
        close(wfd);
        return;
    }
    int i = 0;
    int c = 0;
    file* ctemp;
    file* stemp;
    for(i;i<20;i++)
    {
        ctemp = client->files[i];
        stemp = server->files[i];
        //server has, client does not
        while(ctemp == NULL && stemp!=NULL)
        {
            char* line = myMalloc(strlen(stemp->filename)+strlen(stemp->digest)+10);
            sprintf(line,"A\t%d\t%s\t%s\n",stemp->version,stemp->filename,stemp->digest);
            write(wfd,line,strlen(line));
            printf("A %s\n",stemp->filename);
            free(line);
            stemp = stemp->next;
        }
        //server does not, client has
        while(ctemp!=NULL && stemp == NULL)
        {
            char* line = myMalloc(strlen(ctemp->filename)+strlen(ctemp->digest)+10);
            sprintf(line,"D\t%d\t%s\t%s\n",ctemp->version,ctemp->filename,ctemp->digest);
            write(wfd,line,strlen(line));
            printf("D %s\n",ctemp->filename);
            free(line);
            ctemp = ctemp->next;
        }
        //possible both have
        if(ctemp!=NULL && stemp!=NULL)
        {
            file* cptr = ctemp;
            file* sptr = stemp;
            int exists = 0;
            //check for modify and removes
            while(cptr != NULL)
            {
                while(sptr!=NULL)
                {
                    if(strcmp(cptr->filename,sptr->filename)==0)
                    {
                        exists = 1;
                        break;
                    }
                    sptr=sptr->next;
                }
                if(exists)
                {
                    char* live_hash = getDigest(cptr->filename);
                    //if live has is different from server and client
                    if(strcmp(live_hash,sptr->digest)!=0 && strcmp(live_hash,cptr->digest)!=0)
                    {
                        c = 1;
                        char* con = myMalloc(strlen(project)+11);
                        sprintf(con,"%s/.Conflict",project);
                        int cfd = open(con,O_WRONLY|O_APPEND|O_CREAT,00777);
                        free(con);
                        if(cfd<0)
                        {
                            printf("Error: Could not create a Conflict file. Aborting\n");
                            free(up);
                            freeManifest(server);
                            freeManifest(client);
                            return;
                        }
                        char* line = myMalloc(strlen(sptr->filename)+strlen(live_hash)+20);
                        sprintf(line,"C\t%d\t%s\t%s\n",sptr->version,sptr->filename,live_hash);
                        write(cfd,line,strlen(line));
                        printf("C %s\n",cptr->filename);
                        free(line);
                        // free(live_hash);
                    }
                    //if live is same as client
                    else if(strcmp(cptr->digest,live_hash)==0)
                    {
                        char* line = myMalloc(strlen(sptr->filename)+strlen(sptr->digest)+20);
                        sprintf(line,"M\t%d\t%s\t%s\n",sptr->version,sptr->filename,sptr->digest);
                        write(wfd,line,strlen(line));
                        printf("M %s\n",cptr->filename);
                        free(line);
                        // free(live_hash);
                    }
                }
                else //write a delete
                {
                    char* line = myMalloc(strlen(cptr->filename)+strlen(cptr->digest)+20);
                    sprintf(line,"D\t%d\t%s\t%s\n",cptr->version,cptr->filename,cptr->digest);
                    write(wfd,line,strlen(line));
                    printf("D %s\n",cptr->filename);
                    free(line);
                }
                cptr = cptr->next;
                sptr = stemp;
                exists = 0;
            }
            exists = 0;
            sptr = stemp;
            cptr = ctemp;
            //check for adds
            while(sptr!=NULL)
            {
                while(cptr!=NULL)
                {
                    if(strcmp(sptr->filename,cptr->filename)==0)
                    {
                        exists = 1;
                        break;
                    }
                    cptr = cptr->next;
                }
                if(!exists)
                {
                    char* line = myMalloc(strlen(sptr->filename)+strlen(sptr->digest)+10);
                    sprintf(line,"A\t%d\t%s\t%s\n",sptr->version,sptr->filename,sptr->digest);
                    write(wfd,line,strlen(line));
                    printf("A %s",sptr->filename);
                    free(line);
                }
                sptr = sptr->next;
                cptr = ctemp;
                exists = 0;
            }
        }
    }
    return c;
}

void client_update(char* project,int sock)
{
    struct dirent* dp;
    DIR* dir = opendir(project);
    if(dir==NULL)
    {
        printf("Error: Project %s does not exist locally\n");
        return;
    }
    dp=readdir(dir);
    dp=readdir(dir);
    dp=readdir(dir);
    if(dp!=NULL)
    {
        char* buf = myMalloc(300);
        char* ans = myMalloc(1);
        sprintf(buf,"UPD~%d~%s",strlen(project),project);
        write(sock,buf,strlen(buf));
        free(buf);
        read(sock,ans,1);
        if(atoi(ans)==1)
        {
            node* list = readSocket(sock);
            node* ptr = list;
            manifest* serv_man = initManifest();
            serv_man->version = atoi(ptr->data);
            ptr = ptr->next;
            int i = 1;
            file* temp = initFile();
            //load up server manifest
            while(ptr!=NULL)
            {
                if(strcmp(ptr->data,"\t")!=0 && strcmp(ptr->data,"\n")!=0)
                {
                    if(i%3==0)
                    {
                        temp->digest = myMalloc(strlen(ptr->data)+1);
                        memcpy(temp->digest,ptr->data,strlen(ptr->data));
                        serv_man = insertToManifest(serv_man,temp);
                        temp = initFile();
                        i-=2;
                    }
                    else
                    {
                        if(i%2==0)
                        {
                            temp->filename = myMalloc(strlen(ptr->data)+1);
                            memcpy(temp->filename,ptr->data,strlen(ptr->data));
                        }
                        else
                            temp->version = atoi(ptr->data);
                        i++;
                    }
                }
                ptr = ptr->next;
            }
            if(temp->filename == NULL)
                free(temp);
            char* manPath = myMalloc(strlen(project)+11);
            sprintf(manPath,"%s/.Manifest",project);
            manifest* cli_man = loadManifest(manPath);
            if(cli_man == NULL)
            {
                printf("Error: local manifest is empty\n");
                free(manPath);
                free(ans);
                return;
            }
            free(manPath);
            int x = clientDifference(project,cli_man,serv_man);
            if(x)
            {
                printf("Conflicts were found, please resolve before updating\n");
                char* up = myMalloc(strlen(project)+9);
                sprintf(up,"%s/.Update",project);
                remove(up);
            }
	    printf("Client: update command successful.\n");
        }
        else if(atoi(ans)==2)
            printf("Error: Could not find a .Manifest for project %s. Aborting\n",project);
        else if(atoi(ans)==3)
            printf("Error: Found .Manifest on server, but it was empty.\n");
        else
            printf("Error: The project %s does not exist on the server.\n",project);
        free(ans);
    }
    else
        printf("Error: Project %s is empty\n",project);
    return;
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

void client_upgrade(char* project,int sock)
{
    DIR* dir = opendir(project);
    //check if project exists
    if(dir!=NULL)
    {
        char* con = myMalloc(strlen(project)+11);
        sprintf(con,"%s/.Conflict",project);
        int fd = open(con,O_RDONLY);
        //check if conflict exists
        if(fd>0)
        {
            printf("Error: Please resolve all conflicts and update\n");
            free(con);
            return;
        }
        free(con);
        char* up = myMalloc(strlen(project)+9);
        sprintf(up,"%s/.Update",project);
        fd = open(up,O_RDONLY);
        //check if update exists
        if(fd<0)
        {
            printf("Error: No .Update, please run update on %s\n",project);
            free(up);
            free(dir);
            return;
        }
        char* buf = myMalloc(300);
        char* ans = myMalloc(1);
        sprintf(buf,"UPG~%d~%s",strlen(project),project);
        write(sock,buf,strlen(buf));
        free(buf);
        read(sock,ans,1);
        //exists on server
        if(atoi(ans)==1)
        {
            //read all files from server
            node* list = readSocket(sock);
            //read update file
            node* update = readFile(up);
            if(update == NULL)
            {
                printf("Up To Date\n");
                freeList(list);
                free(ans);
                remove(up);
                return;
            }
            char* manPath = myMalloc(strlen(project)+11);
            sprintf(manPath,"%s/.Manifest",project);
            //read manifest
            manifest* man = loadManifest(manPath);
            if(man==NULL)
            {
                printf("Error: Manifest is empty\n");
                freeList(list);
                free(update);
                free(manPath);
                free(ans);
                return;
            }
            man->version = atoi(list->data);
            //apply updates
            node* uptr = update;
            while(uptr!=NULL)
            {
                if(strcmp(uptr->data,"D")==0)
                {
                    //skip version
                    uptr=uptr->next;
                    uptr=uptr->next;
                    man = deleteFromManifest(man,uptr->data);
                    char* path = myMalloc(strlen(uptr->data)+1);
                    memcpy(path,uptr->data,strlen(uptr->data));
                    remove(path);
                }
                else if(strcmp(uptr->data,"A")==0)
                {
                    file* temp = initFile();
                    uptr = uptr->next;
                    temp->version = atoi(uptr->data);
                    uptr=uptr->next;
                    temp->filename = myMalloc(strlen(uptr->data)+1);
                    memcpy(temp->filename,uptr->data,strlen(uptr->data));
                    uptr=uptr->next;
                    temp->digest = myMalloc(strlen(uptr->data)+1);
                    memcpy(temp->digest,uptr->data,strlen(uptr->data));
                    man = insertToManifest(man,temp);
                    //no guarantee path exists, create it
                    char* path = myMalloc(strlen(temp->filename)+1);
                    memcpy(path,temp->filename,strlen(temp->filename));
                    char* cut = strrchr(path,'/');
                    *cut = '\0';
                    if(mkdirr(path,00777,0) == -1)
                    {
                        printf("Could not create path %s",path);
                        return;
                    }
                    else
                    {
                        *cut = '/';
                        //create file
                        int wfd = open(path,O_WRONLY|O_APPEND|O_CREAT,00777);
                        node* lptr = list;
                        //fill with new data
                        while(lptr!=NULL)
                        {
                            if(strcmp(lptr->data, temp->filename)==0)
                            {
                                //skip the tab after filenames
                                lptr = lptr->next;
                                lptr = lptr->next;
                                while(fd=open(lptr->data,O_WRONLY|O_APPEND)<0)
                                {
                                    write(wfd,lptr->data,strlen(lptr->data));
                                    lptr = lptr->next;
                                    if(lptr==NULL)
                                        break;
                                }
                            }
                            if(lptr==NULL)
                                break;
                            lptr=lptr->next;
                        }
                    }
                }
                else if(strcmp(uptr->data,"M")==0)
                {
                    //create the new file
                    file* temp = initFile();
                    uptr = uptr->next;
                    temp->version = atoi(uptr->data);
                    uptr=uptr->next;
                    temp->filename = myMalloc(strlen(uptr->data)+1);
                    memcpy(temp->filename,uptr->data,strlen(uptr->data));
                    uptr=uptr->next;
                    temp->digest = myMalloc(strlen(uptr->data)+1);
                    memcpy(temp->digest,uptr->data,strlen(uptr->data));
                    //remove local version
                    remove(temp->filename);
                    //remove manifest entry
                    man = deleteFromManifest(man,temp->filename);
                    //replace manifest entry
                    man = insertToManifest(man,temp);
                    //create empty new version
                    int wfd = open(temp->filename,O_WRONLY|O_APPEND|O_CREAT,00777);
                    node* lptr = list;
                    //fill with new data
                    while(lptr!=NULL)
                    {
                        if(strcmp(lptr->data, temp->filename)==0)
                        {
                            lptr = lptr->next;
                            lptr = lptr->next;
                            while(fd=open(lptr->data,O_WRONLY|O_APPEND)<0)
                            {
                                write(wfd,lptr->data,strlen(lptr->data));
                                lptr = lptr->next;
                                if(lptr==NULL)
                                    break;
                            }
                        }
                        if(lptr==NULL)
                            break;
                        lptr=lptr->next;
                    }
                }
                uptr = uptr->next;
            }
            remove(up);
            createManifestFile(man,manPath);
            free(man);
            printf("Client: upgrade command successful.\n");
        }
        else if(atoi(ans)==2)
            printf("Error: Could not find a .Manifest for project %s. Aborting\n",project);
        else if(atoi(ans)==3)
            printf("Error: Found .Manifest on server, but it was empty.\n");
        else
            printf("Error: The project %s does not exist on the server.\n",project);
        free(ans);
    }
    else
        printf("Error: The project %s does not exist locally, cannot upgrade.\n",project);
    return;
}

int clientDifference(char* project, manifest* client, manifest* server)
{
    char* com = myMalloc(strlen(project)+9);
    sprintf(com,"%s/.Commit",project);
    int wfd = open(com,O_WRONLY|O_APPEND|O_CREAT,00777);
    if(wfd<0)
    {
        printf("Error: Could not create a Commit file\n");
        free(com);
        close(wfd);
        return 1;
    }
    if(client->version != server->version)
    {
        printf("Error: Version mismatch, please upgrade the local project\n");
        free(com);
        close(wfd);
        return 1;
    }
    int i = 0;
    int c = 0;
    file* ctemp;
    file* stemp;
    for(i;i<20;i++)
    {
        ctemp = client->files[i];
        stemp = server->files[i];
        //server has, client does not
        while(ctemp == NULL && stemp!=NULL)
        {
            char* line = myMalloc(strlen(stemp->filename)+strlen(stemp->digest)+10);
            sprintf(line,"D\t%d\t%s\t%s\n",stemp->version+1,stemp->filename,stemp->digest);
            write(wfd,line,strlen(line));
            printf("D %s\n",stemp->filename);
            free(line);
            stemp = stemp->next;
            c=0;
        }
        //server does not, client has
        while(ctemp!=NULL && stemp == NULL)
        {
            char* line = myMalloc(strlen(ctemp->filename)+strlen(ctemp->digest)+10);
            sprintf(line,"A\t%d\t%s\t%s\n",ctemp->version,ctemp->filename,ctemp->digest);
            write(wfd,line,strlen(line));
            printf("A %s\n",ctemp->filename);
            free(line);
            ctemp = ctemp->next;
            c=0;
        }
        //possible both have
        if(ctemp!=NULL && stemp!=NULL)
        {
            file* cptr = ctemp;
            file* sptr = stemp;
            int exists = 0;
            //check for modify and add
            while(cptr != NULL)
            {
                while(sptr!=NULL)
                {
                    if(strcmp(cptr->filename,sptr->filename)==0)
                    {
                        exists = 1;
                        break;
                    }
                    sptr=sptr->next;
                }
                if(exists)
                {
                    char* live_hash = getDigest(cptr->filename);
                    //hash sored in both server and client is the same
                    if(strcmp(sptr->digest,cptr->digest)==0)
                    {
                        //live hash is different from stored hash
                        if(strcmp(cptr->digest,live_hash)!=0)
                        {
                            char* line = myMalloc(strlen(sptr->filename)+strlen(live_hash)+20);
                            sprintf(line,"M\t%d\t%s\t%s\n",sptr->version+1,sptr->filename,live_hash);
                            write(wfd,line,strlen(line));
                            printf("M %s\n",cptr->filename);
                            free(line);
                            c=0;
                        }
                        else
                            c=1;
                    }
                    //server hash is different than client
                    else
                    {
                        if(sptr->version>cptr->version)
                        {
                            printf("Error: %s on server is more recent, please synch project\n",cptr->filename);
                            free(client);
                            free(server);
                            free(com);
                            return 1;
                        }
                    }
                    // free(live_hash);
                }
                else //write a delete
                {
                    char* line = myMalloc(strlen(cptr->filename)+strlen(cptr->digest)+20);
                    sprintf(line,"A\t%d\t%s\t%s\n",cptr->version,cptr->filename,cptr->digest);
                    write(wfd,line,strlen(line));
                    printf("A %s\n",cptr->filename);
                    free(line);
                    c=0;
                }
                cptr = cptr->next;
                sptr = stemp;
                exists = 0;
            }
            exists = 0;
            sptr = stemp;
            cptr = ctemp;
            //check for removes
            while(sptr!=NULL)
            {
                while(cptr!=NULL)
                {
                    if(strcmp(sptr->filename,cptr->filename)==0)
                    {
                        exists = 1;
                        break;
                    }
                    cptr = cptr->next;
                }
                if(!exists)
                {
                    char* line = myMalloc(strlen(sptr->filename)+strlen(sptr->digest)+10);
                    sprintf(line,"D\t%d\t%s\t%s\n",sptr->version,sptr->filename,sptr->digest);
                    write(wfd,line,strlen(line));
                    printf("D %s\n",sptr->filename);
                    free(line);
                    c=0;
                }
                sptr = sptr->next;
                cptr = ctemp;
                exists = 0;
            }
        }
    }
    return c;
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

void client_commit(char* project,int sock)
{
    DIR* dir = opendir(project);
    //check if project exists
    if(dir!=NULL)
    {
        char* con = myMalloc(strlen(project)+11);
        sprintf(con,"%s/.Conflict",project);
        int fd = open(con,O_RDONLY);
        //check if conflict exists
        if(fd>0)
        {
            printf("Error: Please resolve all conflicts and update\n");
            free(con);
            return;
        }
        free(con);
        char* up = myMalloc(strlen(project)+9);
        sprintf(up,"%s/.Update",project);
        fd = open(up,O_RDONLY);
        //check if update exists
        if(fd>0)
        {
            //check if empty
            node* update = readFile(up);
            if(update!=NULL)
            {
                printf("Error: There are still pending updates, please upgrade\n");
                free(up);
                return;
            }
        }
        free(up);
        char* buf = myMalloc(300);
        char* ans = myMalloc(1);
        sprintf(buf,"COM~%d~%s",strlen(project),project);
        write(sock,buf,strlen(buf));
        free(buf);
        read(sock,ans,1);
        if(atoi(ans)==1)
        {
            //read server manifest
            char* buff = myMalloc(64);
            int c = 0;
            //get length of message
            while(read(sock,ans,1)>0)
            {
                if(*ans == '~')
                    break;
                memcpy(buff+c,ans,1);
                c++;
            }
            int bytes = atoi(buff);
            //read messsage
            node* list = readSocketL(sock,bytes);
            node* ptr = list;
            manifest* serv_man = initManifest();
            serv_man->version = atoi(ptr->data);
            ptr = ptr->next;
            int i = 1;
            file* temp = initFile();
            //load up server manifest
            while(ptr!=NULL)
            {
                if(strcmp(ptr->data,"\t")!=0 && strcmp(ptr->data,"\n")!=0)
                {
                    if(i%3==0)
                    {
                        temp->digest = myMalloc(strlen(ptr->data)+1);
                        memcpy(temp->digest,ptr->data,strlen(ptr->data));
                        serv_man = insertToManifest(serv_man,temp);
                        temp = initFile();
                        i-=2;
                    }
                    else
                    {
                        if(i%2==0)
                        {
                            temp->filename = myMalloc(strlen(ptr->data)+1);
                            memcpy(temp->filename,ptr->data,strlen(ptr->data));
                        }
                        else
                            temp->version = atoi(ptr->data);
                        i++;
                    }
                }
                ptr = ptr->next;
            }
            //end on an init, gotta free allocated space
            if(temp->filename == NULL)
                free(temp);
            char* manPath = myMalloc(strlen(project)+11);
            sprintf(manPath,"%s/.Manifest",project);
            manifest* cli_man = loadManifest(manPath);
            if(cli_man == NULL)
            {
                printf("Error: local manifest is empty\n");
                free(manPath);
                free(ans);
                return;
            }
            free(manPath);
            int x = clientDifference(project,cli_man,serv_man);
            char* comm = myMalloc(strlen(project)+9);
            sprintf(comm,"%s/.Commit",project);
            if(x)
            {
                remove(comm);
                printf("Nothing to commit, Up To Date\n");
                // free(comm);
                //tell server to not await a commit file
                write(sock,"0",1);
            }
            else
            {
                //tell server commit is coming
                write(sock,"1",1);
                char* hash = getDigest(comm);
                char* file = myMalloc(strlen(hash)+8);
                sprintf(file,".Commit%s\t",hash);
                //write .commit<hash> to tell apart different hashes
                write(sock,file,strlen(file));
                int fd = open(comm,O_RDONLY);
                char* c = myMalloc(1);
                while(read(fd,c,1)>0)
                    write(sock,c,1);
                free(file);
                free(c);
                close(fd);
                // free(comm);
		printf("Client: commit command successful.\n");
            }
        }
        else if(atoi(ans)==2)
            printf("Error: Could not find a .Manifest for project %s. Aborting\n",project);
        else if(atoi(ans)==3)
            printf("Error: Found .Manifest on server, but it was empty.\n");
        else
            printf("Error: The project %s does not exist on the server.\n",project);
        free(ans);
    }
    else
        printf("Error: The project %s does not exist locally, cannot commit.\n",project);
    return;
}

void client_history(char* file, int sock)
{
    //max file name length is 255, need 4 for create flag, need 4 max for number of bytes
    char* buf = myMalloc(300);
    char* ans = myMalloc(1);
    //construct a message in the format HIS~#bytes~filename
    sprintf(buf,"HIS~%d~%s",strlen(file),file);
    write(sock,buf,strlen(buf));
    free(buf);
    read(sock,ans,1);
    if(atoi(ans)==1)
    {
        node* list = readSocket(sock);
        node* ptr = list->next;
        //skip the filename that is sent over
        ptr = ptr->next;
        while(ptr!=NULL)
        {
            printf("%s",ptr->data);
            ptr = ptr->next;
        }
	printf("Client: history command successful.\n");
        freeList(list);
    }
    else if(atoi(ans)==2)
        printf("Error: Could not find .history on server\n");
    else
        printf("Error: Project %s does not exist on server\n",file);
    free(ans);
}

void client_rollback(char* file,char* version, int sock)
{
    //max file name length is 255, need 4 for create flag, need 4 max for number of bytes
    char* buf = myMalloc(300);
    char* ans = myMalloc(1);
    //construct a message in the format ROL~#bytes~filename
    sprintf(buf,"ROL~%d~%s",strlen(file),file);
    write(sock,buf,strlen(buf));
    free(buf);
    //check if the project is there
    read(sock,ans,1);
    if(atoi(ans)==1)
    {
        //send version number
        char* ver = myMalloc(strlen(version)+1);
        sprintf(ver,"%s~",version);
        write(sock,ver,strlen(ver));
        //check status on version #
        read(sock,ans,1);
        if(atoi(ans)==1)
            printf("Client: Project %s has been rollbacked to version %s\n",file,version);
        else
            printf("Error: Invalid version number to rollback\n");
    }
    else
        printf("Error: Project %s does not exist on server\n",file);
    free(ans);
    return;
}

void client_push(char* project,int sock)
{
    DIR* dir = opendir(project);
    //check if project exists
    if(dir!=NULL)
    {
        char* com = myMalloc(strlen(project)+11);
        sprintf(com,"%s/.Commit",project);
        int fd = open(com,O_RDONLY);
        //check if commit exists
        if(fd<0)
        {
            printf("Error: There is no .Commit file, please run commit\n");
            free(com);
            return;
        }
        char* buf = myMalloc(300);
        char* ans = myMalloc(1);
        sprintf(buf,"PSH~%d~%s",strlen(project),project);
        write(sock,buf,strlen(buf));
        free(buf);
        read(sock,ans,1);
        //project exists on server
        if(atoi(ans)==1)
        {
            int msg = 0;
            //load up commit
            node* commit = readFile(com);
            if(commit==NULL)
            {
                printf("Error: .Commit is empty\n");
                free(ans);
                return;
            }
            //hash the commit to check for duplicate
            char* hash = myMalloc(strlen(project)+100);
            sprintf(hash,"%s/commit/.Commit%s\t",project,getDigest(com));
            //add length of hash
            msg+=strlen(hash);
            //find size of incoming message
            node* cptr = commit;
            while(cptr!=NULL)
            {
                if(strcmp(cptr->data,"A")==0 || strcmp(cptr->data,"M")==0)
                {
                    //skip operation
                    cptr = cptr->next;
                    //skip version
                    cptr = cptr->next;
                    //add filename and tab
                    msg+=strlen(cptr->data)+1;
                    char* r = myMalloc(1);
                    int fd = open(cptr->data,O_RDONLY);
                    while(read(fd,r,1)>0)
                        msg++;
                    free(r);
                }
                cptr = cptr->next;
            }
            char* msgsize = myMalloc(100);
            sprintf(msgsize,"%d~",msg);
            write(sock,msgsize,strlen(msgsize));
            //now write out file hash and files
            write(sock,hash,strlen(hash));
            free(hash);
            cptr = commit;
            while(cptr!=NULL)
            {
                if(strcmp(cptr->data,"A")==0 || strcmp(cptr->data,"M")==0)
                {
                    //skip version
                    cptr = cptr->next;
                    cptr = cptr->next;
                    //if file exists send it
                    if((fd = open(cptr->data,O_RDONLY)>0))
                        writeFile(cptr->data,sock);
                }
                cptr = cptr->next;
            }
            read(sock,ans,1);
            //failure check
            if(atoi(ans)==0)
            {
                //success, so modify client manifest
                char* manPath = myMalloc(strlen(project)+11);
                sprintf(manPath,"%s/.Manifest",project);
                manifest* man = loadManifest(manPath);
                cptr = commit;
                file* temp;
                while(cptr!=NULL)
                {
                    //dont care about A or D because those changes
                    //already exist for them to be able to be committed
                    if(strcmp(cptr->data,"M")==0)
                    {
                        //create a temp
                        temp = initFile();
                        cptr = cptr->next;
                        temp->version = atoi(cptr->data);
                        cptr = cptr->next;
                        temp->filename = myMalloc(strlen(cptr->data)+1);
                        memcpy(temp->filename,cptr->data,strlen(cptr->data));
                        cptr = cptr->next;
                        temp->digest = myMalloc(strlen(cptr->data)+1);
                        memcpy(temp->digest,cptr->data,strlen(cptr->data));
                        //delete old version
                        man = deleteFromManifest(man,temp->filename);
                        //insert the new
                        man = insertToManifest(man,temp);
                    }
                    cptr = cptr->next;
                }
                //increment manifest
                man->version+=1;
                //create new manifest
                createManifestFile(man,manPath);
                free(manPath);
                freeManifest(man);
		printf("Client: push command successful.\n");
            }
            else if(atoi(ans)==2)
                printf("Error: Could not find a .Manifest for project %s. Aborting\n",project);
            else if(atoi(ans)==3)
                printf("Error: Found .Manifest on server, but it was empty.\n");
            else if(atoi(ans)==4)
                printf("Error: Server does not have a corresponding .Commit, please run commit\n");
            else
                printf("Error: Something went wrong serverside, please check server terminal\n");
            remove(com);
            free(ans);
            free(com);
        }
        else
            printf("Error: The project %s does not exist on the server.\n",project);
        free(ans);
    }
    else
        printf("Error: The project %s does not exist locally, cannot commit.\n",project);
    return;
}

int main(int argc, char** argv)
{
    if(argc == 4)
    {
        if(strcmp(argv[1],"configure")==0)
            config(argv[2],argv[3]);
        else if(strcmp(argv[1],"add")==0)
            add(argv[2],argv[3]);
        else if(strcmp(argv[1],"remove")==0)
            rem(argv[2],argv[3]);
        else if(strcmp(argv[1],"rollback")==0)
        {
            int net_sock = initSocket();
            client_rollback(argv[2],argv[3], net_sock);
	    printf("Client: disconnected from server.\n");
            close(net_sock);
        }
    }
    else if(argc == 3)
    {
        if(strcmp(argv[1],"configure")==0 || strcmp(argv[1],"add")==0 || strcmp(argv[1],"remove")==0)
            printf("Error, expected 4 arguments, received %d\n",argc);
        if(strcmp(argv[1],"create")==0)
        {
            int net_sock = initSocket();
            client_creat(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");            
	    close(net_sock);
        }
        else if(strcmp(argv[1],"destroy")==0)
        {
            int net_sock = initSocket();
            client_des(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");            
            close(net_sock);
        }
        else if(strcmp(argv[1],"currentversion")==0)
        {
            int net_sock = initSocket();
            client_ver(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");            
	    close(net_sock);
        }
        else if(strcmp(argv[1],"checkout")==0)
        {
            int net_sock = initSocket();
            client_check(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");            
	    close(net_sock);
        }
        else if(strcmp(argv[1],"update")==0)
        {
            int net_sock = initSocket();
            client_update(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");            
	    close(net_sock);
        }
        else if(strcmp(argv[1],"upgrade")==0)
        {
            int net_sock = initSocket();
            client_upgrade(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");		
            close(net_sock);
        }
        else if(strcmp(argv[1],"commit")==0)
        {
            int net_sock = initSocket();
            client_commit(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");        
	    close(net_sock);
        }
        else if(strcmp(argv[1],"push")==0)
        {
            int net_sock = initSocket();
            client_push(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");	 
            close(net_sock);
        }
        else if(strcmp(argv[1],"history")==0)
        {
            int net_sock = initSocket();
            client_history(argv[2],net_sock);
	    printf("Client: disconnected from server.\n");           
	    close(net_sock);
        }
    }
    else
        printf("Error: Expected at least 2 arguments, received %d",argc);
    return 0;
}
