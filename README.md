# ST-Hsm-Zephyr-Cpp

----

### Description:
This project aims to implement an HSM on an ST MCU and eventually adapt Federated Learning (FL) to run on it. The HSM provides a secure environment for storing key pairs and offering cryptographic libraries. FL is designed to utilize the HSM to protect local data during decentralized training. Therefore, the roadmap below outlines the necessary steps to achieve these goals. 

---

### Step-by-step Implementation
| Step | Description |
|------|-------------|
| 1. Implement MPU | Apply memory protection for secure storage. |
| 2. Implement Key Management | Manage and securely store cryptographic keys. |
| 3. Implement Secure Boot | Verify boot firmware integrity during startup. |
| 4. Implement Secure Debug | Restrict access to the debug port. |
| 5. Implement Secure Flash (FW update) | Allow firmware updates only from authenticated sources. |
| 6. Implement Secure Lock | Lock memory to prevent tampering or exploitation. |
| 7. Port Zephyr | Port the implemented HSM onto the Zephyr RTOS platform. |
| 8. Abstract HSM using C++ | Build an abstraction layer in C++ to interface with the HSM. |
| 9. Implement Basic FL | Use the HSM to protect local data in Federated Learning. |

---

### Development Environment
1. MCU: STM32F407V6T6U
2. EVB: STM32F407G-DISC1
3. UART: CP2102
4. IDE: STM32CubeIDE 1.19.0