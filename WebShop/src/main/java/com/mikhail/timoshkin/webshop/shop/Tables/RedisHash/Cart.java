package com.mikhail.timoshkin.webshop.shop.Tables.RedisHash;

import lombok.Data;
import org.springframework.data.redis.core.RedisHash;

import java.util.HashMap;
import java.util.Map;

@RedisHash("Cart")
@Data
public class Cart {
    private Integer id; // client id
    private Integer sum = 0;
    private Map<Integer, Item> cart = new HashMap<>();

    @Override
    public String toString() {
        StringBuilder stringBuilder = new StringBuilder("\n");
        for (Item item: cart.values()) {
            stringBuilder.append(item).append("\n");
        }
        return stringBuilder.append("Total worth: ").append(sum).append("\n").toString();
    }
}
