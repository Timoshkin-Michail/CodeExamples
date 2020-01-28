package com.mikhail.timoshkin.webshop.shop;

import com.mikhail.timoshkin.webshop.Commands;
import com.mikhail.timoshkin.webshop.Message;
import com.mikhail.timoshkin.webshop.shop.Tables.ClientCarts;
import com.mikhail.timoshkin.webshop.shop.Tables.RedisHash.Cart;
import com.mikhail.timoshkin.webshop.shop.Tables.RedisHash.Category;
import com.mikhail.timoshkin.webshop.shop.Tables.RedisHash.Item;
import com.mikhail.timoshkin.webshop.shop.Tables.Store;
import com.mikhail.timoshkin.webshop.shop.Tables.StoreCategories;
import lombok.extern.slf4j.Slf4j;
import org.springframework.amqp.rabbit.annotation.RabbitListener;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.context.ApplicationContext;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Profile;

import java.util.*;
import java.util.stream.Collectors;

@Slf4j
@Profile("server")
@Configuration
public class Server implements ApplicationRunner {

    private final Scanner scanner = new Scanner(System.in);
    private final Store store;
    private final StoreCategories storeCategories;
    private final ClientCarts clientCarts;
    private final ApplicationContext applicationContext;

    private Integer itemSeq = 1;
    private Integer clientSeq = 1;

    private Set<Integer> purchases = new HashSet<>();




    public Server(Store store, StoreCategories storeCategories, ClientCarts clientCarts, ApplicationContext applicationContext) {
        this.store = store;
        this.storeCategories = storeCategories;
        this.clientCarts = clientCarts;
        this.applicationContext = applicationContext;
    }


    @RabbitListener(queues = "requests_q")
    public String response(Message message) {
        log.info(" [x] Received request {}", message);
        try {
            switch (message.getCommand()) {
                case Commands.Client.register:
                    return register();
                case Commands.Client.add_to_cart:
                    return add_to_cart(message.getId(), message.getContent());
                case Commands.Client.rem_from_cart:
                    return rem_from_cart(message.getId(), message.getContent());
                case Commands.Client.buy:
                    return buy(message.getId());
                case Commands.Client.discard:
                    return discard(message.getId());
                case Commands.Client.show_all:
                    return show_all();
                case Commands.Client.show_category:
                    return show_category(message.getContent());
                case Commands.Client.list_categories:
                    return list_categories();
                case Commands.Client.statistics:
                    return statistics();
                case Commands.Client.exit:
                    return exit(message.getId());
                default:
                    return "Unknown command\n";
            }
        } catch (Exception e) {
            return e.getMessage();
        }
    }

    @Override
    public void run(ApplicationArguments args){
        String command = "";
        log.trace("STORE: {}", store.findAll());
        log.trace("STORE_CATEGORIES: {}", storeCategories.findAll());
        while (!Commands.Server.stop_server.equals(command) && scanner.hasNextLine()) {
            command = scanner.nextLine();
            if (Commands.Server.add_item.equals(command)) {
                add_item();
            }
        }
        log.info("Stopping server...");
        store.deleteAll();
        storeCategories.deleteAll();
        SpringApplication.exit(applicationContext, () -> 0);
        System.exit(0);
    }

    public String register() {
        Cart cart = new Cart();
        cart.setId(clientSeq);
        cart.setCart(new HashMap<>());
        cart.setSum(0);
        clientCarts.save(cart);
        return (clientSeq++).toString();
    }

    public String statistics() {
        Float averageCartSum = 0.0f;
        int cartSize = 0;
        Iterable<Cart> allCarts = clientCarts.findAll();
        for (Cart cart: allCarts) {
            averageCartSum += cart.getSum();
            cartSize++;
        }
        if (cartSize != 0) {
            averageCartSum /= 1.0f * cartSize;
        }
        Float averageItemCount = 0.0f;
        int storeSize = 0;
        Iterable<Item> allItems = store.findAll();
        for (Item item: allItems) {
            averageItemCount += item.getCount();
            storeSize++;
        }
        if (storeSize != 0) {
            averageItemCount /= storeSize;
        }
        return "\n_______Statistics________:\n" + "Average Cart Sum: " + averageCartSum + "\n" +
                "Average Item Count: " + averageItemCount + "\n" +
                "Purchase: " + purchases.size() + "\n" +
                "Shop visitors: " + (clientSeq - 1) + "\n";
    }

    public String add_to_cart(Integer clientId, String content) {
        try {
            String[] items = content.split(" ");
            Cart clientCart = clientCarts.findById(clientId).get();
            String updateCart = requireUpdate(clientCart);
            if (updateCart != null) {
                return updateCart;
            }
            boolean changed = false;
            for (String itemStr : items) {
                int itemId;
                Integer count;
                if (itemStr.contains("x")) {
                    itemId = Integer.parseInt(itemStr.substring(0, itemStr.indexOf("x")));
                    count = Integer.valueOf(itemStr.substring(itemStr.indexOf("x") + 1));
                } else {
                    itemId = Integer.parseInt(itemStr);
                    count = 1;
                }
                Optional<Item> itemOpt = store.findById(itemId);
                if (itemOpt.isEmpty()) {
                    return "There is no item with ID = " + itemId;
                } else {
                    Item itemInStore = itemOpt.get();
                    if (clientCart.getCart().containsKey(itemId)) {
                        Item existingItem = clientCart.getCart().get(itemId);
                        if (count + existingItem.getCount() > itemInStore.getCount()) {
                            return "We couldn't offer you additional " + count + "things of " + itemInStore.getName() + ". You can add only "
                                    + (itemInStore.getCount() - existingItem.getCount()) + " " + itemInStore.getName() + "s";
                        } else {
                            existingItem.setCount(existingItem.getCount() + count);
                        }
                        clientCart.setSum(clientCart.getSum() + count * itemInStore.getPrice());
                        clientCart.getCart().put(itemId, existingItem);
                        changed = true;
                    } else if (itemInStore.getCount() < count) {
                        return "There are not enough " + itemInStore.getName() + "s in the store. We can offer only " + itemInStore.getCount() + " of it";
                    } else {
                        Item newItemCart = new Item();
                        newItemCart.setId(itemId);
                        newItemCart.setCount(count);
                        newItemCart.setName(itemInStore.getName());
                        newItemCart.setCategories(itemInStore.getCategories());
                        newItemCart.setPrice(itemInStore.getPrice());
                        clientCart.setSum(clientCart.getSum() + count * newItemCart.getPrice());
                        clientCart.getCart().put(itemId, newItemCart);
                        changed = true;
                    }
                }
            }
            if (!changed) {
                return "List of items to add is empty";
            } else {
                clientCarts.save(clientCart);
                return "\n Items are successfully added. Your cart:\n" + clientCart.toString();
            }
        } catch (Exception e) {
            return "Error";
        }
    }

    public String rem_from_cart(Integer clientId, String content) {
        String[] items = content.split(" ");
        Cart clientCart = clientCarts.findById(clientId).get();
        String updateCart = requireUpdate(clientCart);
        if (updateCart != null) {
            return updateCart;
        }
        boolean changed = false;
        for (String itemStr: items) {
            int itemId;
            Integer count;
            if (itemStr.contains("x")) {
                itemId = Integer.parseInt(itemStr.substring(0, itemStr.indexOf("x")));
                count = Integer.valueOf(itemStr.substring(itemStr.indexOf("x") + 1));
            } else {
                itemId = Integer.parseInt(itemStr);
                count = 1;
            }
            if (!clientCart.getCart().containsKey(itemId)) {
                return "You don't have any items with ID = " + itemId + " in your cart";
            }
            Item existing = clientCart.getCart().get(itemId);
            if (existing.getCount() < count) {
                return "You have less items in your cart than you want to remove";
            } else if (existing.getCount().equals(count)) {
                clientCart.getCart().remove(itemId);
                clientCart.setSum(clientCart.getSum() - count * existing.getPrice());
                changed = true;
            } else {
                existing.setCount(existing.getCount() - count);
                clientCart.setSum(clientCart.getSum() - count * existing.getPrice());
                clientCart.getCart().put(itemId, existing);
                changed = true;
            }
        }
        if (!changed) {
            return "List of items to remove is empty";
        } else {
            clientCarts.save(clientCart);
            return "\n Items are successfully removed. Your cart:\n" + clientCart.toString();
        }
    }

    public String buy(Integer clientId) {
        Cart clientCart = clientCarts.findById(clientId).get();
        if (clientCart.getCart().isEmpty()) {
            return "Your cart is empty";
        }
        String update = requireUpdate(clientCart);
        if (update != null) {
            return update;
        } else {
            for (Item local: clientCart.getCart().values()) {
                Item remote = store.findById(local.getId()).get();
                if (remote.getCount().equals(local.getCount())) {
                    store.deleteById(remote.getId());
                    local.getCategories().forEach(category -> {
                        Category c = storeCategories.findById(category).get();
                        c.getItemIds().remove(remote.getId());
                        if (c.getItemIds().isEmpty()) {
                            storeCategories.deleteById(category);
                        } else {
                            storeCategories.save(c);
                        }
                    });
                } else {
                    remote.setCount( remote.getCount() - local.getCount());
                    store.save(remote);
                }
            }
            clientCart.getCart().clear();
            clientCarts.save(clientCart);
            purchases.add(clientId);
            return "You successfully bought your items!";
        }
    }

    public String requireUpdate(Cart clientCart) {
        List<Integer> remove = new ArrayList<>();
        List<Item> change =  new ArrayList<>();
        StringBuilder stringBuilder = new StringBuilder("\nSorry, you are too late.");
        boolean requireUpdate = false;
        for (Item local: clientCart.getCart().values()) {
            Optional<Item> remote = store.findById(local.getId());
            if (remote.isEmpty() || remote.get().getCount() < local.getCount()) {
                requireUpdate = true;
                if (remote.isEmpty()) {
                    stringBuilder.append("All ").append(local.getName()).append("s are already sold.");
                    remove.add(local.getId());
                } else {
                    stringBuilder.append("There are only ").append(remote.get().getCount()).append(" ")
                            .append(local.getName()).append("s left in the store.");
                    change.add(remote.get());
                }
            }
        }
        if (requireUpdate) {
            for (Integer id: remove) {
                clientCart.getCart().remove(id);
            }
            for (Item itemToChange: change) {
                clientCart.getCart().get(itemToChange.getId()).setCount(itemToChange.getCount());
            }
            clientCarts.save(clientCart);
            return stringBuilder.append("We updated your cart to actual status").append(clientCart).toString();
        } else {
            return null;
        }
    }

    public String discard(Integer clientId) {
        Cart clientCart = clientCarts.findById(clientId).get();
        clientCart.getCart().clear();
        clientCart.setSum(0);
        clientCarts.save(clientCart);
        return "\nYour cart successfully discarded\n";
    }

    public String exit(Integer clientId) {
        clientCarts.deleteById(clientId);
        return "See you soon!";
    }

    public void add_item(){
        log.info("Enter item's name, price, count, categories");
        String itemInfo = scanner.nextLine().strip();
        Scanner itemScanner = new Scanner(itemInfo);
        itemScanner.useDelimiter(" ");
        Item newItem = new Item();
        newItem.setId(itemSeq++);
        newItem.setName(itemScanner.next());
        newItem.setPrice(itemScanner.nextInt());
        newItem.setCount(itemScanner.nextInt());
        List<String> categories = new ArrayList<>();
        while (itemScanner.hasNext()){
            categories.add(itemScanner.next());
        }
        for (String category: categories) {
            Optional<Category> cat = storeCategories.findById(category);
            Category c;
            c = cat.orElseGet(Category::new);
            c.getItemIds().add(newItem.getId());
            c.setId(category);
            storeCategories.save(c);
        }
        log.trace("STORE_CATEGORIES: {}", storeCategories.findAll());
        newItem.setCategories(categories);
        store.save(newItem);
        log.info("Item {} successfully added", newItem);
        log.trace("STORE: {}", store.findAll());
    }

    public String show_all() {
        Iterable<Item> items = store.findAll();
        return makeStringResponse(items);
    }

    public String show_category(String category) {
        Optional<Category> itemIdsOpt = storeCategories.findById(category);
        if (itemIdsOpt.isEmpty()) {
            return "There aren't any items with such category: " + category;
        }
        List<Integer> itemIds = itemIdsOpt.get().getItemIds();
        log.trace("Item Ids: {}", itemIds);
        List<String> items = new ArrayList<>();
        itemIds.forEach(itemId -> {
            Optional<Item> item = store.findById(itemId);
            item.ifPresent(value -> items.add(value.toString()));
        });
        return makeStringResponse(items);
    }

    public String list_categories() {
        Set<String> distinctCategories = new HashSet<>();
        Iterable<Category> categories = storeCategories.findAll();
        categories.forEach(category -> {
            distinctCategories.add(category.getId());
        });
        return makeStringResponse(distinctCategories);
    }

    private <T extends Iterable> String makeStringResponse(T array) {
        StringBuilder stringBuilder = new StringBuilder("\n");
        array.forEach(o -> stringBuilder.append(o.toString()).append("\n"));
        return stringBuilder.toString();
    }
}
