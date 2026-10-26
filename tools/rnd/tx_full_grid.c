#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
static volatile uint32_t *r;
static volatile sig_atomic_t stop;
static const char *dbg="/sys/kernel/debug/iio/iio:device0/";
static const char *sys="/sys/bus/iio/devices/iio:device0/";
static void sig(int x){(void)x;stop=1;}
static int put(const char *base,const char *name,const char *s){
 char p[512];snprintf(p,sizeof p,"%s%s",base,name);FILE *f=fopen(p,"w");if(!f)return -1;
 int n=fprintf(f,"%s\n",s),e=fclose(f);return n<0||e ? -1:0;
}
static int get(const char *base,const char *name,char *s,size_t n){
 char p[512];snprintf(p,sizeof p,"%s%s",base,name);FILE *f=fopen(p,"r");if(!f)return -1;
 char *v=fgets(s,(int)n,f);int e=fclose(f);if(!v||e)return -1;s[strcspn(s,"\r\n")]=0;return 0;
}
static int spi_get(unsigned a,unsigned *v){
 char b[64];snprintf(b,sizeof b,"0x%x",a);
 if(put(dbg,"direct_reg_access",b)||get(dbg,"direct_reg_access",b,sizeof b))return -1;
 *v=(unsigned)strtoul(b,NULL,0);return 0;
}
static int spi_set(unsigned a,unsigned v){char b[64];snprintf(b,sizeof b,"0x%x 0x%x",a,v);return put(dbg,"direct_reg_access",b);}
static void ms(unsigned m){struct timespec t={(time_t)(m/1000),(long)(m%1000)*1000000};while(nanosleep(&t,&t)&&errno==EINTR&&!stop){}}
static unsigned check(unsigned wait,unsigned v[4]){
 for(unsigned c=0;c<4;c++)r[(0x404+64*c)/4]=6;
 __sync_synchronize();ms(wait);unsigned z=0;
 for(unsigned c=0;c<4;c++){v[c]=r[(0x404+64*c)/4];z|=v[c];}
 return z;
}
int main(int argc,char **argv){
 unsigned test_rx=argc>1?(unsigned)strtoul(argv[1],NULL,0):0xffffffffu;
 int axis=argc>2&&!strcmp(argv[2],"axis");
 if(test_rx!=0xffffffffu&&test_rx>255)return 2;
 unsigned save[4][4],rx=0,tx=0,v[4],pass=0;int modified=0,rc=1;
 char gain0[80],gain1[80],mode[80],lb[80],prbs[80],freq[80],name[80];
 signal(SIGINT,sig);signal(SIGTERM,sig);setvbuf(stdout,NULL,_IOLBF,0);
 int fd=open("/dev/mem",O_RDWR|O_SYNC);if(fd<0){perror("devmem");return 1;}
 r=mmap(NULL,0x5000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0x79020000);close(fd);if(r==MAP_FAILED){perror("mmap");return 1;}
 if(r[0]!=0xa0300||r[0x4000/4]!=0x90162){fprintf(stderr,"Unexpected core version\n");goto end;}
 if(get(sys,"out_voltage0_hardwaregain",gain0,sizeof gain0)||get(sys,"out_voltage1_hardwaregain",gain1,sizeof gain1)||get(sys,"ensm_mode",mode,sizeof mode)||get(sys,"in_voltage_sampling_frequency",freq,sizeof freq)||get(dbg,"loopback",lb,sizeof lb)||get(dbg,"bist_prbs",prbs,sizeof prbs)||spi_get(6,&rx)||spi_get(7,&tx))goto end;
 if(get(sys,"name",name,sizeof name)||strcmp(name,"ad9361-phy")||!(r[0x5c/4]&1)||r[0x4040/4]!=3||(r[0x4048/4]&32)||r[0x404c/4]!=3){fprintf(stderr,"Core mode/activity precondition mismatch\n");goto end;}
 if(strcmp(mode,"fdd")||strcmp(lb,"0")||strcmp(prbs,"0")||strcmp(freq,"40000000")){fprintf(stderr,"Baseline precondition mismatch\n");goto end;}
 /* Gain sysfs accepts its numeric component, without the printed dB suffix. */
 gain0[strcspn(gain0," ")]=0;gain1[strcspn(gain1," ")]=0;
 for(unsigned c=0;c<4;c++){save[c][0]=r[(0x400+64*c)/4];save[c][1]=r[(0x418+64*c)/4];save[c][2]=r[(0x4414+64*c)/4];save[c][3]=r[(0x4418+64*c)/4];}
 printf("BASELINE rx=%02x tx=%02x freq=%s mode=%s gain=%s,%s\n",rx,tx,freq,mode,gain0,gain1);
 modified=1;
 if(put(sys,"out_voltage0_hardwaregain","-89.75")||put(sys,"out_voltage1_hardwaregain","-89.75"))goto restore;
 if(test_rx!=0xffffffffu){if(put(sys,"ensm_mode","alert")||spi_set(6,test_rx)||put(sys,"ensm_mode","fdd"))goto restore;}
 printf("TEST_RX=%02x axis=%d\n",test_rx==0xffffffffu?rx:test_rx,axis);
 if(put(dbg,"bist_prbs","2"))goto restore;
 for(unsigned c=0;c<4;c++){r[(0x400+64*c)/4]=0x251;r[(0x418+64*c)/4]=save[c][1]&~0xf0000u;}
 unsigned bad=check(20,v);
 printf("RX_CONTROL,%u,%u,%u,%u,adc=%08x\n",v[0],v[1],v[2],v[3],r[0x5c/4]);
 if(bad||!(r[0x5c/4]&1)){fprintf(stderr,"RX PN9 positive control failed; no TX scan\n");goto restore;}
 if(put(dbg,"bist_prbs","0")||put(dbg,"loopback","1"))goto restore;
 for(unsigned c=0;c<4;c++){r[(0x418+64*c)/4]=(save[c][1]&~0xf0000u)|0x90000;r[(0x4414+64*c)/4]=0;r[(0x4418+64*c)/4]=9;}
 r[0x4044/4]=1;__sync_synchronize();
 printf("clock,data,status0,status1,status2,status3,adc,confirm500ms\n");
 for(unsigned clk=0;clk<16&&!stop;clk++){
  if(axis&&clk!=0&&clk!=15)continue;
  if(put(sys,"ensm_mode","alert")||spi_set(7,clk<<4)||put(sys,"ensm_mode","fdd"))goto restore;
  for(unsigned data=0;data<16&&!stop;data++){
   if(spi_set(7,(clk<<4)|data))goto restore;
   bad=check(5,v);unsigned confirm=0xffffffffu;
   if(!bad){unsigned vv[4];confirm=check(500,vv);if(!confirm&&!stop&&(r[0x5c/4]&1))pass++;}
   printf("%u,%u,%u,%u,%u,%u,%08x,%08x\n",clk,data,v[0],v[1],v[2],v[3],r[0x5c/4],confirm);
  }
 }
 printf("CONFIRMED_PASS_POINTS=%u\n",pass);rc=stop?2:0;
restore:
 if(modified){
  int err=0;
  err|=put(sys,"ensm_mode","alert");err|=spi_set(6,rx);err|=spi_set(7,tx);
  for(unsigned c=0;c<4;c++){r[(0x400+64*c)/4]=save[c][0];r[(0x418+64*c)/4]=save[c][1];r[(0x4414+64*c)/4]=save[c][2];r[(0x4418+64*c)/4]=save[c][3];}
  r[0x4044/4]=1;__sync_synchronize();
  err|=put(dbg,"loopback",lb);err|=put(dbg,"bist_prbs",prbs);
  err|=put(sys,"ensm_mode",mode);
  err|=put(sys,"out_voltage0_hardwaregain",gain0);err|=put(sys,"out_voltage1_hardwaregain",gain1);
  unsigned a=0,b=0;err|=spi_get(6,&a);err|=spi_get(7,&b);
  for(unsigned c=0;c<4;c++)if(r[(0x400+64*c)/4]!=save[c][0]||r[(0x418+64*c)/4]!=save[c][1]||r[(0x4414+64*c)/4]!=save[c][2]||r[(0x4418+64*c)/4]!=save[c][3])err=1;
  char rb[80];
  if(get(sys,"ensm_mode",rb,sizeof rb)||strcmp(rb,mode))err=1;
  if(get(dbg,"loopback",rb,sizeof rb)||strcmp(rb,lb))err=1;
  if(get(dbg,"bist_prbs",rb,sizeof rb)||strcmp(rb,prbs))err=1;
  if(get(sys,"out_voltage0_hardwaregain",rb,sizeof rb)||strtod(rb,NULL)!=strtod(gain0,NULL))err=1;
  if(get(sys,"out_voltage1_hardwaregain",rb,sizeof rb)||strtod(rb,NULL)!=strtod(gain1,NULL))err=1;
  if(err||a!=rx||b!=tx){fprintf(stderr,"RESTORE_FAILED\n");rc=3;}else printf("RESTORE_OK rx=%02x tx=%02x\n",a,b);
 }
end: munmap((void*)r,0x5000);return rc;
}
