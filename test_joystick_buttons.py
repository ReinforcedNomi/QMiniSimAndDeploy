#!/usr/bin/env python3
"""
测试手柄按键编号映射
用于检测机械狮 G5 pro V2 手柄的按键编号
"""

import pygame
import time

def test_joystick_buttons():
    pygame.init()
    pygame.joystick.init()
    
    # 检测手柄数量
    joystick_count = pygame.joystick.get_count()
    if joystick_count == 0:
        print("未检测到手柄！")
        return
    
    print(f"检测到 {joystick_count} 个手柄")
    
    # 初始化第一个手柄
    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    
    print(f"\n手柄名称: {joystick.get_name()}")
    print(f"按键数量: {joystick.get_numbuttons()}")
    print(f"摇杆数量: {joystick.get_numaxes()}")
    print(f"方向键数量: {joystick.get_numhats()}")
    
    print("\n" + "="*60)
    print("请按下手柄上的各个按键，程序会显示对应的按键编号")
    print("按 Ctrl+C 退出")
    print("="*60 + "\n")
    
    # 记录按键状态
    button_states = [False] * joystick.get_numbuttons()
    
    try:
        while True:
            for event in pygame.event.get():
                if event.type == pygame.JOYBUTTONDOWN:
                    button_id = event.button
                    button_states[button_id] = True
                    print(f"按键按下: button[{button_id}]")
                    
                elif event.type == pygame.JOYBUTTONUP:
                    button_id = event.button
                    button_states[button_id] = False
                    print(f"按键释放: button[{button_id}]")
                
                elif event.type == pygame.JOYAXISMOTION:
                    axis_id = event.axis
                    axis_value = joystick.get_axis(axis_id)
                    if abs(axis_value) > 0.1:  # 只显示有明显变化的
                        print(f"摇杆移动: axis[{axis_id}] = {axis_value:.3f}")
                
                elif event.type == pygame.JOYHATMOTION:
                    hat_id = event.hat
                    hat_value = joystick.get_hat(hat_id)
                    print(f"方向键: hat[{hat_id}] = {hat_value}")
            
            # 显示当前所有按键状态
            active_buttons = [i for i, state in enumerate(button_states) if state]
            if active_buttons:
                print(f"当前按下的按键: {active_buttons}", end='\r')
            
            time.sleep(0.01)
            
    except KeyboardInterrupt:
        print("\n\n测试结束")
        print("\n按键映射建议:")
        print("根据上面的测试结果，找到以下按键对应的编号:")
        print("  A 键 -> button[?]")
        print("  B 键 -> button[?]")
        print("  X 键 -> button[?]")
        print("  Y 键 -> button[?]")
        print("  L1 键 -> button[?]")
        print("  R1 键 -> button[?]")
        print("  L2 键 -> button[?]")
        print("  R2 键 -> button[?]")
        print("  SELECT 键 -> button[?]")
        print("  START 键 -> button[?]")
    
    pygame.quit()

if __name__ == "__main__":
    test_joystick_buttons()

