package com.mikhail.timoshkin.webshop;

import org.springframework.amqp.core.*;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;


@Configuration
public class RabbitMqConfiguration {

        @Bean
        public Queue queue() {
            return new Queue("requests_q");
        }

        @Bean
        public DirectExchange exchange() {
            return new DirectExchange("requests");
        }

        @Bean
        public Binding binding(DirectExchange exchange,
                               Queue queue) {
            return BindingBuilder.bind(queue)
                    .to(exchange)
                    .with("rpc");
        }
}