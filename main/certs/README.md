# Certificados AWS IoT

Esta pasta deve conter os certificados para conexão com AWS IoT Core:

## Arquivos necessários:
- `aws-root-ca.pem` - Certificado raiz da AWS
- `certificate.pem.crt` - Certificado do dispositivo
- `private.pem.key` - Chave privada do dispositivo

## Como obter:
1. Acesse AWS IoT Console
2. Navegue para Manage -> Things
3. Selecione seu dispositivo
4. Gere e baixe os certificados
5. Cole os arquivos nesta pasta

## Segurança:
- Estes arquivos NÃO devem ser commitados no Git
- Mantenha os certificados em local seguro
- Revogue certificados comprometidos
