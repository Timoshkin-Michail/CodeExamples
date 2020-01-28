package com.mikhail.timoshkin.webshop.costumer;

import com.mikhail.timoshkin.webshop.Commands;
import com.mikhail.timoshkin.webshop.Message;
import lombok.extern.slf4j.Slf4j;
import org.springframework.amqp.core.DirectExchange;
import org.springframework.amqp.rabbit.core.RabbitTemplate;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.convert.ApplicationConversionService;
import org.springframework.context.ApplicationContext;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;
import org.springframework.core.convert.ConversionService;

import java.util.Scanner;

@Slf4j
@Profile("client")
@Configuration
public class Customer implements ApplicationRunner {

    private final ConversionService conversionService = ApplicationConversionService.getSharedInstance();

    private Integer myId;

    private final RabbitTemplate template;
    private final DirectExchange exchange;
    private final ApplicationContext applicationContext;


    @SuppressWarnings("SpringJavaInjectionPointsAutowiringInspection")
    public Customer(RabbitTemplate template, DirectExchange exchange, ApplicationContext applicationContext) {
        this.template = template;
        this.exchange = exchange;
        this.applicationContext = applicationContext;
    }

    @Override
    public void run(ApplicationArguments args){
        Scanner scanner = new Scanner(System.in);
        log.trace("Registration...");
        Message m = new Message();
        m.setCommand(Commands.Client.register);
        myId = conversionService.convert(template.convertSendAndReceive(exchange.getName(), "rpc", m), Integer.class);
        log.trace("Registration: success. Got id = {}", myId);
        while (scanner.hasNext()) {
            Message message = new Message();
            message.setId(myId);
            String rawInput = scanner.nextLine().trim();
            if (rawInput.contains(" ")) {
                message.setCommand(rawInput.substring(0, rawInput.indexOf(' ')));
                message.setContent(rawInput.substring(rawInput.indexOf(' ') + 1));
            } else {
                message.setCommand(rawInput);
            }
            log.trace(" [x] Sending {}", message);
            String response = (String) template.convertSendAndReceive
                    (exchange.getName(), "rpc", message);
            log.trace(" [.] Got {}", response);
            log.info("{}", response);
            if (Commands.Client.exit.equals(message.getCommand())) {
                break;
            }
        }
        SpringApplication.exit(applicationContext, () -> 0);
        System.exit(0);
    }
}
