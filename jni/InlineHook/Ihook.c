#include "Ihook.h"
#include "fixPCOpcode.h"

#define ALIGN_PC(pc)	(pc & 0xFFFFFFFC)

/**
 * �޸�ҳ���ԣ��ĳɿɶ���д��ִ��
 * @param   pAddress   ��Ҫ�޸�������ʼ��ַ
 * @param   size       ��Ҫ�޸�ҳ���Եĳ��ȣ�byteΪ��λ
 * @return  bool       �޸��Ƿ�ɹ�
 */
bool ChangePageProperty(void *pAddress, size_t size)
{
    bool bRet = false;
    
    if(pAddress == NULL)
    {
        LOGI("change page property error.");
        return bRet;
    }
    
    //���������ҳ����������ʼ��ַ
    unsigned long ulPageSize = sysconf(_SC_PAGESIZE); //�õ�ҳ�Ĵ�С
    int iProtect = PROT_READ | PROT_WRITE | PROT_EXEC;
    unsigned long ulNewPageStartAddress = (unsigned long)(pAddress) & ~(ulPageSize - 1); //pAddress & 0x1111 0000 0000 0000
    long lPageCount = (size / ulPageSize) + 1;
    
    long l = 0;
    while(l < lPageCount)
    {
        //����mprotect��ҳ����
        int iRet = mprotect((const void *)(ulNewPageStartAddress), ulPageSize, iProtect);
        if(-1 == iRet)
        {
            LOGI("mprotect error:%s", strerror(errno));
            return bRet;
        }
        l++; 
    }
    
    return true;
}

/**
 * ͨ��/proc/$pid/maps����ȡģ���ַ
 * @param   pid                 ģ�����ڽ���pid���������������̣�����С��0��ֵ����-1
 * @param   pszModuleName       ģ������
 * @return  void*               ģ���ַ�������򷵻�0
 */
void * GetModuleBaseAddr(pid_t pid, char* pszModuleName)
{
    FILE *pFileMaps = NULL;
    unsigned long ulBaseValue = 0;
    char szMapFilePath[256] = {0};
    char szFileLineBuffer[1024] = {0};

    LOGI("Pid is %d\n",pid);

    //pid�жϣ�ȷ��maps�ļ�
    if (pid < 0)
    {
        snprintf(szMapFilePath, sizeof(szMapFilePath), "/proc/self/maps");
    }
    else
    {
        snprintf(szMapFilePath, sizeof(szMapFilePath),  "/proc/%d/maps", pid);
    }

    pFileMaps = fopen(szMapFilePath, "r");
    if (NULL == pFileMaps)
    {
        return (void *)ulBaseValue;
    }

    LOGI("Get map.\n");

    //ѭ������maps�ļ����ҵ���Ӧģ�飬��ȡ��ַ��Ϣ
    while (fgets(szFileLineBuffer, sizeof(szFileLineBuffer), pFileMaps) != NULL)
    {
        //LOGI("%s\n",szFileLineBuffer);
        //LOGI("%s\n",pszModuleName);
        if (strstr(szFileLineBuffer, pszModuleName))
        {
            LOGI("%s\n",szFileLineBuffer);
            char *pszModuleAddress = strtok(szFileLineBuffer, "-");
            if (pszModuleAddress)
            {
                ulBaseValue = strtoul(pszModuleAddress, NULL, 16);

                if (ulBaseValue == 0x8000)
                    ulBaseValue = 0;

                break;
            }
        }
    }
    fclose(pFileMaps);
    return (void *)ulBaseValue;
}

/**
 * arm��inline hook������Ϣ���ݣ�����ԭ�ȵ�opcodes��
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               ��ʼ����Ϣ�Ƿ�ɹ�
 */
bool InitArmHookInfo(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    uint32_t *currentOpcode = pstInlineHook->pHookAddr;

    for(int i=0;i<BACKUP_CODE_NUM_MAX;i++){
        pstInlineHook->backUpFixLengthList[i] = -1;
    }
    
    if(pstInlineHook == NULL)
    {
        LOGI("pstInlineHook is null");
        return bRet;
    }

    pstInlineHook->backUpLength = 8;
    
    memcpy(pstInlineHook->szbyBackupOpcodes, pstInlineHook->pHookAddr, pstInlineHook->backUpLength);

    for(int i=0;i<2;i++){
        currentOpcode += i;
        LOGI("Arm32 Opcode to fix %d : %x",i,*currentOpcode);
        LOGI("Fix length : %d",lengthFixArm32(*currentOpcode));
        pstInlineHook->backUpFixLengthList[i] = lengthFixArm32(*currentOpcode);
    }
    
    return true;
}

bool InitThumbHookInfo(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    int backUpPos = 0;
    uint16_t *currentOpcode = pstInlineHook->pHookAddr-1;
    int cnt = 0;
    int is_thumb32_count=0;

    for(int i=0;i<BACKUP_CODE_NUM_MAX;i++){
        pstInlineHook->backUpFixLengthList[i] = -1;
    }
    
    if(pstInlineHook == NULL)
    {
        LOGI("pstInlineHook is null");
        return bRet;
    }

    uint16_t *p11; 
    
    //�ж������(pHookAddr-1)[10:11]��ɵ�thumb�����ǲ���thumb32��
    //����ǵĻ�����Ҫ����14byte����10byte����ʹ�û��ָ����ضϡ�������תָ���ڲ�nop�������Ҳֻ��Ҫ10byte��
    //���Ծ�ȡpstInlineHook->backUpLengthΪ10

    for (int k=5;k>=0;k--){
        p11 = pstInlineHook->pHookAddr-1+k*2;
        LOGI("P11 : %x",*p11);
        if(isThumb32(*p11)){
            is_thumb32_count += 1;
        }else{
            break;
        }
    }

    LOGI("is_thumb32_count : %d",is_thumb32_count);
    
    if(is_thumb32_count%2==1)
    {
        LOGI("The last ins is thumb32. Length will be 10.");
        pstInlineHook->backUpLength = 10;
    }
    else{
        LOGI("The last ins is not thumb32. Length will be 12.");
        pstInlineHook->backUpLength = 12;
    }

    //����������szbyBackupOpcodes�����ƫ��1 byte
    memcpy(pstInlineHook->szbyBackupOpcodes, pstInlineHook->pHookAddr-1, pstInlineHook->backUpLength); 

    while(1)
    {
        LOGI("Hook Info Init");
        //int cnt=0;
        if(isThumb32(*currentOpcode))
        {
            LOGI("cnt %d thumb32",cnt);
            uint16_t *currentThumb32high = currentOpcode;
            uint16_t *currentThumb32low = currentOpcode+1;
            uint32_t instruction;
            int fixLength;

            instruction = (*currentThumb32high<<16) | *currentThumb32low;
            fixLength = lengthFixThumb32(instruction);
            LOGI("fixLength : %d",fixLength);
            pstInlineHook->backUpFixLengthList[cnt++] = 1; //˵���Ǹ�thumb32
            pstInlineHook->backUpFixLengthList[cnt++] = fixLength - 1;
            backUpPos += 4;
        }
        else{
            LOGI("cnt %d thumb16",cnt);
            uint16_t instruction = *currentOpcode;
            int fixLength;
            fixLength = lengthFixThumb16(instruction);
            LOGI("fixLength : %d",fixLength);
            pstInlineHook->backUpFixLengthList[cnt++] = fixLength;
            backUpPos += 2;
        }

        if (backUpPos < pstInlineHook->backUpLength)
        {
            currentOpcode = pstInlineHook->pHookAddr -1 + sizeof(uint8_t)*backUpPos;
            LOGI("backUpPos : %d", backUpPos);
        }
        else{
            return true;
        }
    }
    
    return false;
}

/**
 * ����ihookstub.s�е�shellcode����׮����ת��pstInlineHook->onCallBack�����󣬻ص��Ϻ���
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               inlinehook׮�Ƿ���ɹ�
 */
bool BuildStub(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        
        void *p_shellcode_start_s = &_shellcode_start_s;
        void *p_shellcode_end_s = &_shellcode_end_s;
        void *p_hookstub_function_addr_s = &_hookstub_function_addr_s;
        void *p_old_function_addr_s = &_old_function_addr_s;

        size_t sShellCodeLength = p_shellcode_end_s - p_shellcode_start_s;
        //mallocһ���µ�stub����
        void *pNewShellCode = malloc(sShellCodeLength);
        if(pNewShellCode == NULL)
        {
            LOGI("shell code malloc fail.");
            break;
        }
        memcpy(pNewShellCode, p_shellcode_start_s, sShellCodeLength);
        //����stub����ҳ���ԣ��ĳɿɶ���д��ִ��
        if(ChangePageProperty(pNewShellCode, sShellCodeLength) == false)
        {
            LOGI("change shell code page property fail.");
            break;
        }

        //������ת���ⲿstub����ȥ
        void **ppHookStubFunctionAddr = pNewShellCode + (p_hookstub_function_addr_s - p_shellcode_start_s);
        *ppHookStubFunctionAddr = pstInlineHook->onCallBack;
        
        //�����ⲿstub�������������ת�ĺ�����ַָ�룬��������Ϻ������µ�ַ
        pstInlineHook->ppOldFuncAddr  = pNewShellCode + (p_old_function_addr_s - p_shellcode_start_s);
            
        //���shellcode��ַ��hookinfo�У����ڹ���hook��λ�õ���תָ��
        pstInlineHook->pStubShellCodeAddr = pNewShellCode;

        bRet = true;
        break;
    }
    
    return bRet;
}

//����Ŀ�꺯����thumb������_old_function_addr_s��Ҫ����+1���л���thumbģʽ����������ִ����ת֮�������ԭʼ��thumbָ��
bool BuildStubThumb(INLINE_HOOK_INFO* pstInlineHook) 
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        
        void *p_shellcode_start_s = &_shellcode_start_s_thumb;
        void *p_shellcode_end_s = &_shellcode_end_s_thumb;
        void *p_hookstub_function_addr_s = &_hookstub_function_addr_s_thumb;
        void *p_old_function_addr_s = &_old_function_addr_s_thumb;

        size_t sShellCodeLength = p_shellcode_end_s - p_shellcode_start_s;
        //mallocһ���µ�stub����
        void *pNewShellCode = malloc(sShellCodeLength);
        if(pNewShellCode == NULL)
        {
            LOGI("shell code malloc fail.");
            break;
        }
        memcpy(pNewShellCode, p_shellcode_start_s, sShellCodeLength);
        //����stub����ҳ���ԣ��ĳɿɶ���д��ִ��
        if(ChangePageProperty(pNewShellCode, sShellCodeLength) == false)
        {
            LOGI("change shell code page property fail.");
            break;
        }

        //������ת���ⲿstub����ȥ
        void **ppHookStubFunctionAddr = pNewShellCode + (p_hookstub_function_addr_s - p_shellcode_start_s);
        *ppHookStubFunctionAddr = pstInlineHook->onCallBack;
        
        //�����ⲿstub�������������ת�ĺ�����ַָ�룬��������Ϻ������µ�ַ
        pstInlineHook->ppOldFuncAddr  = pNewShellCode + (p_old_function_addr_s - p_shellcode_start_s); //�������+1
            
        //���shellcode��ַ��hookinfo�У����ڹ���hook��λ�õ���תָ��
        pstInlineHook->pStubShellCodeAddr = pNewShellCode;

        bRet = true;
        break;
    }
    
    return bRet;
}


/**
 * ���첢���ARM��32����תָ���Ҫ�ⲿ��֤�ɶ���д����pCurAddress����8��bytes��С
 * @param  pCurAddress      ��ǰ��ַ��Ҫ������תָ���λ��
 * @param  pJumpAddress     Ŀ�ĵ�ַ��Ҫ�ӵ�ǰλ������ȥ�ĵ�ַ
 * @return                  ��תָ���Ƿ���ɹ�
 */
bool BuildArmJumpCode(void *pCurAddress , void *pJumpAddress)
{
    bool bRet = false;
    while(1)
    {
        if(pCurAddress == NULL || pJumpAddress == NULL)
        {
            LOGI("address null.");
            break;
        }        
        //LDR PC, [PC, #-4]
        //addr
        //LDR PC, [PC, #-4]��Ӧ�Ļ�����Ϊ��0xE51FF004
        //addrΪҪ��ת�ĵ�ַ������תָ�ΧΪ32λ������32λϵͳ��˵��Ϊȫ��ַ��ת��
        //���湹��õ���תָ�ARM��32λ������ָ��ֻ��Ҫ8��bytes��
        BYTE szLdrPCOpcodes[8] = {0x04, 0xF0, 0x1F, 0xE5};
        //��Ŀ�ĵ�ַ��������תָ���λ��
        memcpy(szLdrPCOpcodes + 4, &pJumpAddress, 4);
        
        //������õ���תָ��ˢ��ȥ
        memcpy(pCurAddress, szLdrPCOpcodes, 8);
        cacheflush(*((uint32_t*)pCurAddress), 8, 0);
        
        bRet = true;
        break;
    }
    return bRet;
}

bool BuildThumbJumpCode(void *pCurAddress , void *pJumpAddress)
{
    bool bRet = false;
    while(1)
    {
        if(pCurAddress == NULL || pJumpAddress == NULL)
        {
            LOGI("address null.");
            break;
        }        
        //LDR PC, [PC, #0]
        //addr
        //LDR PC, [PC, #0]��Ӧ��thumb������Ϊ��0xf004f85f//arm��LDR PC, [PC, #-4]Ϊ0xE51FF004
        //addrΪҪ��ת�ĵ�ַ������תָ�ΧΪ32λ������32λϵͳ��˵��Ϊȫ��ַ��ת��
        //���湹��õ���תָ�ARM��32λ������ָ��ֻ��Ҫ8��bytes��
        //����Ŀ�������thumb-2ָ���˵��ʹ�ù̶���8����12byte�����ǿ϶�������ģ���Ϊthumb16��2byte����thumb32��4byte���ǻ��ʹ�õ�
        //��ˣ�������12byteʱ�����Ŀ����2+2+2+2+2+4���Ǿͻ�������Ǹ�thumb32�ضϡ�
        //������8byteʱ�����Ŀ����2+4+4��Ҳ�������thumb32�ض�
        if (CLEAR_BIT0((uint32_t)pCurAddress) % 4 != 0) {
			//((uint16_t *) CLEAR_BIT0(pCurAddress))[i++] = 0xBF00;  // NOP
            BYTE szLdrPCOpcodes[12] = {0x00, 0xBF, 0xdF, 0xF8, 0x00, 0xF0};
            memcpy(szLdrPCOpcodes + 6, &pJumpAddress, 4);
            memcpy(pCurAddress, szLdrPCOpcodes, 10);
            cacheflush(*((uint32_t*)pCurAddress), 10, 0);
		}
        else{
            BYTE szLdrPCOpcodes[8] = {0xdF, 0xF8, 0x00, 0xF0};
            //��Ŀ�ĵ�ַ��������תָ���λ��
            memcpy(szLdrPCOpcodes + 4, &pJumpAddress, 4);
            memcpy(pCurAddress, szLdrPCOpcodes, 8);
            cacheflush(*((uint32_t*)pCurAddress), 8, 0);
        }

        
        
        //������õ���תָ��ˢ��ȥ
        //memcpy(pCurAddress, szLdrPCOpcodes, 8); //�����Ҫ�ο�ele7enxxh�Ĵ��룬����nop֮���
        //cacheflush(*((uint32_t*)pCurAddress), 8, 0);
        
        bRet = true;
        break;
    }
    return bRet;
}

/**
 * ���챻inline hook�ĺ���ͷ����ԭԭ����ͷ+������ת
 * ���ǿ�����ת���ɣ�ͬʱ���stub shellcode�е�oldfunction��ַ��hookinfo�����old������ַ
 * ���ʵ��û��ָ���޸����ܣ�����HOOK��λ��ָ����漰PC����Ҫ�ض���ָ��
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               ԭ���������Ƿ�ɹ�
 */
bool BuildOldFunction(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;

    void *fixOpcodes;
    int fixLength;

    fixOpcodes = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        
        //8��bytes���ԭ����opcodes������8��bytes�����ת��hook���������תָ��
        void * pNewEntryForOldFunction = malloc(200);
        if(pNewEntryForOldFunction == NULL)
        {
            LOGI("new entry for old function malloc fail.");
            break;
        }

        pstInlineHook->pNewEntryForOldFunction = pNewEntryForOldFunction;
        
        if(ChangePageProperty(pNewEntryForOldFunction, 200) == false)
        {
            LOGI("change new entry page property fail.");
            break;
        }
        
        fixLength = fixPCOpcodeArm(fixOpcodes, pstInlineHook); //�ѵ������ֵ���ʼ��ַ����ȥ
        memcpy(pNewEntryForOldFunction, fixOpcodes, fixLength);
        //memcpy(pNewEntryForOldFunction, pstInlineHook->szbyBackupOpcodes, 8);
        //�����תָ��
        if(BuildArmJumpCode(pNewEntryForOldFunction + fixLength, pstInlineHook->pHookAddr + pstInlineHook->backUpLength) == false)
        {
            LOGI("build jump opcodes for new entry fail.");
            break;
        }
        //���shellcode��stub�Ļص���ַ
        *(pstInlineHook->ppOldFuncAddr) = pNewEntryForOldFunction;
        
        bRet = true;
        break;
    }
    
    return bRet;
}











bool BuildOldFunctionThumb(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;

    void *fixOpcodes;
    int fixLength;

    fixOpcodes = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        
        //12��bytes���ԭ����thumb opcodes������8��bytes�����ת��hook���������תָ��
        void * pNewEntryForOldFunction = malloc(200);
        if(pNewEntryForOldFunction == NULL)
        {
            LOGI("new entry for old function malloc fail.");
            break;
        }

        pstInlineHook->pNewEntryForOldFunction = pNewEntryForOldFunction;
        
        if(ChangePageProperty(pstInlineHook->pNewEntryForOldFunction, 200) == false)
        {
            LOGI("change new entry page property fail.");
            break;
        }
        
        fixLength = fixPCOpcodeThumb(fixOpcodes, pstInlineHook); //�޸�PC���ָ��
        //�����޸���opcode��ָ��ȣ��޸����ָ�����fixOpcode��
        memcpy(pNewEntryForOldFunction, fixOpcodes, fixLength);
        //memcpy(pNewEntryForOldFunction, pstInlineHook->szbyBackupOpcodes, pstInlineHook->backUpLength);
        LOGI("pHookAddr : %x",pstInlineHook->pHookAddr);
        LOGI("backupLength : %x",pstInlineHook->backUpLength);
        //�����תָ��
        if(BuildThumbJumpCode(pNewEntryForOldFunction + fixLength, pstInlineHook->pHookAddr + pstInlineHook->backUpLength) == false)
        {
            LOGI("build jump opcodes for new entry fail.");
            break;
        }
        //���shellcode��stub�Ļص���ַ
        *(pstInlineHook->ppOldFuncAddr) = pNewEntryForOldFunction;
        
        bRet = true;
        break;
    }
    
    return bRet;
}
    
/**
 * ��ҪHOOK��λ�ã�������ת����ת��shellcode stub��
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               ԭ����תָ���Ƿ���ɹ�
 */
bool RebuildHookTarget(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        //�޸�ԭλ�õ�ҳ���ԣ���֤��д
        if(ChangePageProperty(pstInlineHook->pHookAddr, 8) == false)
        {
            LOGI("change page property error.");
            break;
        }
        //�����תָ��
        if(BuildArmJumpCode(pstInlineHook->pHookAddr, pstInlineHook->pStubShellCodeAddr) == false)
        {
            LOGI("build jump opcodes for new entry fail.");
            break;
        }
        bRet = true;
        break;
    }
    
    return bRet;
}

bool RebuildHookTargetThumb(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        //�޸�ԭλ�õ�ҳ���ԣ���֤��д
        if(ChangePageProperty(pstInlineHook->pHookAddr, 8) == false)
        {
            LOGI("change page property error.");
            break;
        }
        //�����תָ��
        if(BuildThumbJumpCode(pstInlineHook->pHookAddr-1, pstInlineHook->pStubShellCodeAddr) == false)
        {
            LOGI("build jump opcodes for new entry fail.");
            break;
        }
        bRet = true;
        break;
    }
    
    return bRet;
}
/**
 * ARM�µ�inlinehook
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               inlinehook�Ƿ����óɹ�
 */
bool HookArm(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    LOGI("HookArm()");
    
    while(1)
    {
        //LOGI("pstInlineHook is null 1.");
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null.");
            break;
        }

        //LOGI("Init Arm HookInfo fail 1.");
        //����ARM��inline hook�Ļ�����Ϣ
        if(InitArmHookInfo(pstInlineHook) == false)
        {
            LOGI("Init Arm HookInfo fail.");
            break;
        }
        
        //LOGI("BuildStub fail 1.");
        //����stub�������Ǳ���Ĵ���״̬��ͬʱ��ת��Ŀ�꺯����Ȼ����ת��ԭ����
        //��ҪĿ���ַ������stub��ַ��ͬʱ����oldָ���������� 
        if(BuildStub(pstInlineHook) == false)
        {
            LOGI("BuildStub fail.");
            break;
        }
        
        //LOGI("BuildOldFunction fail 1.");
        //�����ع�ԭ����ͷ���������޸�ָ�������ת�ص�ԭ��ַ��
        //��Ҫԭ������ַ
        if(BuildOldFunction(pstInlineHook) == false)
        {
            LOGI("BuildOldFunction fail.");
            break;
        }
        
        //LOGI("RebuildHookAddress fail 1.");
        //������дԭ����ͷ��������ʵ��inline hook�����һ������д��ת
        //��Ҫcacheflush����ֹ����
        if(RebuildHookTarget(pstInlineHook) == false)
        {
            LOGI("RebuildHookAddress fail.");
            break;
        }
        bRet = true;
        break;
    }

    return bRet;
}

/**
 * Thumb16 Thumb32�µ�inlinehook
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               inlinehook�Ƿ����óɹ�
 */
bool HookThumb(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    LOGI("HookThumb()");
    
    while(1)
    {
        //LOGI("pstInlineHook is null 1.");
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null.");
            break;
        }

        //LOGI("Init Thumb HookInfo fail 1.");
        //����ARM��inline hook�Ļ�����Ϣ
        if(InitThumbHookInfo(pstInlineHook) == false)
        {
            LOGI("Init Arm HookInfo fail.");
            break;
        }
        
        //LOGI("BuildStub fail 1.");
        //����stub�������Ǳ���Ĵ���״̬��ͬʱ��ת��Ŀ�꺯����Ȼ����ת��ԭ����
        //��ҪĿ���ַ������stub��ַ��ͬʱ����oldָ���������� 
        if(BuildStubThumb(pstInlineHook) == false)
        {
            LOGI("BuildStub fail.");
            break;
        }
        
        //LOGI("BuildOldFunction fail 1.");
        //�����ع�ԭ����ͷ���������޸�ָ�������ת�ص�ԭ��ַ��
        //��Ҫԭ������ַ
        if(BuildOldFunctionThumb(pstInlineHook) == false)
        {
            LOGI("BuildOldFunction fail.");
            break;
        }
        
        //LOGI("RebuildHookAddress fail 1.");
        //������дԭ����ͷ��������ʵ��inline hook�����һ������д��ת
        //��Ҫcacheflush����ֹ����
        if(RebuildHookTargetThumb(pstInlineHook) == false)
        {
            LOGI("RebuildHookAddress fail.");
            break;
        }
        bRet = true;
        break;
    }

    return bRet;
}
