
#pragma once 

enum Code_First
{
  O = 0,
  H = 1, // 烙铁头 温度模式
  C = 2, // 烙铁芯 温度模式
  E = 3, // 错误
  S = 4, // 设置
  U = 5, // 升温异常
};

//------------手柄选择【1硬件选择模式 】--------------------
enum HandleType
{
  Handle_None = 0, // 外部悬空
  A1321 = 1,       // 外部 4.7K 上拉
  C245 = 2,        // 外部 4.7K 下拉
};
/**
 * @brief  升级版错误代码生成器
 * @param  errNum  错误编号 (传入 1 到 99 的整数)
 * @param  code   错误 代码  E 设备 /U 升温
 * @param  isPublic 是否是通用设备  是否强制第二位为 0 (默认为 false, 传入 true 则强制为 0)
 * @return 返回标准的 4 位错误代码字符串
 */
const char *generateErrorCode(int errNum, String code, bool isPublic)
{
  // 1. 确定第二位
  if (isPublic)
  {
    code += "0"; // 如果传入了强制标记，直接生成 0
  }
  else
  {

    // 否则根据硬件类型动态识别
    if ( getCurrent().handleType == C245)
    {
      code += "2";
    }
    if ( getCurrent().handleType == A1321)
    {
      code += "1";
    }
    if ( getCurrent().handleType == 0)
    {
      code += "0";
    }
  }

  // 2. 处理后两位的数字，自动用 '0' 补齐
  if (errNum < 10)
  {
    code += "0" + String(errNum);
  }
  else
  {
    code += String(errNum);
  }

  // 3. 安全防御截断
  if (code.length() > 4)
  {
    code = code.substring(0, 4);
  }

  return code.c_str();
} 
 